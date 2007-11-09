/*
 * Copyright (C) 1995-2007, Index Data ApS
 * See the file LICENSE for details.
 *
 * $Id: poll.c,v 1.1 2007-11-09 16:46:43 adam Exp $
 */
/**
 * \file 
 * \brief Select, poll wrapper
 */

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <yaz/log.h>
#include <yaz/xmalloc.h>
#include <yaz/poll.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef WIN32
#if FD_SETSIZE < 512
#define FD_SETSIZE 512
#endif
#include <winsock.h>
#endif

/*
  Note that yaz_poll_select is limited as to how many file
  descriptors it can multiplex due to its use of select() which in
  turn uses the statically defined fd_set type to be a bitmap of the
  file descriptors to check.  On Ubuntu 6.06 (and almost certainly on
  Debian, and probably on all Linuxes, and maybe all Unixes) this is
  by default set to 1024 (though it may be possible to override this
  using a #define before including <sys/select.h> -- I've not tried
  this).  1024 file descriptors is a lot, but not enough in all
  cases, e.g. when running IRSpy on a large target database.  So you
  should ensure that YAZ uses ZOOM_yaz_poll_poll() when possible.
*/
int yaz_poll_select(struct yaz_poll_fd *fds, int num_fds, int timeout)
{
    struct timeval tv;
    fd_set input, output, except;
    int i, r;
    int max_fd = 0;

    FD_ZERO(&input);
    FD_ZERO(&output);
    FD_ZERO(&except);

    assert(num_fds > 0);
    for (i = 0; i < num_fds; i++)
    {
        enum yaz_poll_mask mask = fds[i].input_mask;
        int fd = fds[i].fd;

        if (mask & yaz_poll_read)
            FD_SET(fd, &input);
        if (mask & yaz_poll_write)
            FD_SET(fd, &output);
        if (mask & yaz_poll_except)
            FD_SET(fd, &except);
        if (max_fd < fd)
            max_fd = fd;
    }
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    while ((r = select(max_fd+1, &input, &output, &except,
                       (timeout == -1 ? 0 : &tv))) < 0 && errno == EINTR)
    {
        ;
    }
    for (i = 0; i < num_fds; i++)
    {
        enum yaz_poll_mask mask = 0;
        int fd = fds[i].fd;
        if (!r)
            mask += yaz_poll_timeout;
        else
        {
            if (FD_ISSET(fd, &input))
                mask += yaz_poll_read;
            if (FD_ISSET(fd, &output))
                mask += yaz_poll_write;
            if (FD_ISSET(fd, &except))
                mask += yaz_poll_except;
        }
        fds[i].output_mask = mask;
    }
    return r;
}

#if HAVE_SYS_POLL_H
int yaz_poll_poll(struct yaz_poll_fd *fds, int num_fds, int timeout)
{
    int r;
    struct pollfd *pollfds = (struct pollfd *) 
        xmalloc(num_fds * sizeof *pollfds);
    int i;

    assert(num_fds > 0);
    for (i = 0; i < num_fds; i++)
    {
        enum yaz_poll_mask mask = fds[i].input_mask;
        int fd = fds[i].fd;
        short poll_events = 0;

        if (mask & yaz_poll_read)
            poll_events += POLLIN;
        if (mask & yaz_poll_write)
            poll_events += POLLOUT;
        if (mask & yaz_poll_except)
            poll_events += POLLERR;
        pollfds[i].fd = fd;
        pollfds[i].events = poll_events;
        pollfds[i].revents = 0;
    }
    while ((r = poll(pollfds, num_fds,
                     (timeout == -1 ? -1 : timeout * 1000))) < 0
           && errno == EINTR)
    {
        ;
    }
    if (r >= 0)
    {
        for (i = 0; i < num_fds; i++)
        {
            enum yaz_poll_mask mask = 0;
            if (!r)
                mask += yaz_poll_timeout;
            else
            {
                if (pollfds[i].revents & POLLIN)
                    mask += yaz_poll_read;
                if (pollfds[i].revents & POLLOUT)
                    mask += yaz_poll_write;
                if (pollfds[i].revents & POLLERR)
                    mask += yaz_poll_except;
            }
            fds[i].output_mask = mask;
        }
    }
    xfree(pollfds);
    return r;
}
#endif

int yaz_poll(struct yaz_poll_fd *fds, int num_fds, int timeout)
{
#if YAZ_HAVE_SYS_POLL_H
    return yaz_poll_poll(fds, num_fds, timeout);
#else
    return yaz_poll_select(fds, num_fds, timeout);
#endif
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

