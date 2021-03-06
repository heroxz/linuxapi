/*
 * This file is part of the linuxapi package.
 * Copyright (C) 2011-2013 Mark Veltzer <mark.veltzer@gmail.com>
 *
 * linuxapi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * linuxapi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with linuxapi. If not, see <http://www.gnu.org/licenses/>.
 */

#include <firstinclude.h>
#include <pthread.h>	// for pthread_setcancelstate(3)
#include <unistd.h>	// for sleep(3)
#include <err_utils.h>	// for CHECK_ZERO_ERRNO()
#include <trace_utils.h>// for TRACE()

/*
 * This demo is a pthread_cancel demo and was copied from the pthread_cancel
 * manpage.
 *
 * EXTRA_LINK_FLAGS=-lpthread
 *
 * TODO:
 * - show that the cancellation happened in the middle of the sleep and that it was close
 * to the time that asked the cancellation.
 * - show what other functions instead of sleep can be used as cancellation points.
 */

static void * thread_func(void *ignored_argument) {
	/* Disable cancellation for a while, so that we don't
	 * immediately react to a cancellation request */
	CHECK_ZERO_ERRNO(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL));
	TRACE("started; cancellation disabled");
	CHECK_ZERO(sleep(5));
	TRACE("about to enable cancellation");
	CHECK_ZERO_ERRNO(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL));
	/* sleep(3) is a cancellation point */
	CHECK_ZERO(sleep(1000));/* Should get canceled while we sleep */
	/* Should never get here */
	TRACE("not canceled!");
	return NULL;
}

int main(int argc, char** argv, char** envp) {
	pthread_t thr;
	void *res;

	/* Start a thread and then send it a cancellation request */

	CHECK_ZERO_ERRNO(pthread_create(&thr, NULL, &thread_func, NULL));
	CHECK_ZERO(sleep(2));	/* Give thread a chance to get started */
	TRACE("sending cancellation request");
	CHECK_ZERO_ERRNO(pthread_cancel(thr));
	/* Join with thread to see what its exit status was */
	CHECK_ZERO_ERRNO(pthread_join(thr, &res));
	if (res==PTHREAD_CANCELED)
		TRACE("thread was canceled");
	else
		TRACE("thread wasn't canceled (shouldn't happen!)");
	return EXIT_SUCCESS;
}
