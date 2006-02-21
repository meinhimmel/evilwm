/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2006 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <X11/Xproto.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "evilwm.h"
#include "log.h"

int need_client_tidy = 0;
int ignore_xerror = 0;

/* Now do this by fork()ing twice so we don't have to worry about SIGCHLDs */
void spawn(const char *const cmd[]) {
	Window cur_root, dw;
	int di;
	unsigned int dui;
	ScreenInfo *current_screen;
	pid_t pid;

	/* XQueryPointer is useful for getting the current pointer root */
	XQueryPointer(dpy, screens[0].root, &cur_root, &dw, &di, &di, &di, &di, &dui);
	current_screen = find_screen(cur_root);
	if (current_screen && current_screen->display)
		putenv(current_screen->display);
	if (!(pid = fork())) {
		setsid();
		switch (fork()) {
			/* Expect compiler warnings because of half-broken SUS
			 * execvp prototype:  "char *const argv[]" should have
			 * been "const char *const argv[]", but the committee
			 * favored legacy code over modern code, and modern
			 * compilers bark at our extra const. (LD) */
			case 0: execvp(cmd[0], cmd+1);
			default: _exit(0);
		}
	}
	if (pid > 0)
		wait(NULL);
}

void handle_signal(int signo) {
	(void)signo;  /* unused */
	int i;
	/* SIGCHLD check no longer necessary */
	/* Quit Nicely */
	while(head_client) remove_client(head_client);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	if (font) XFreeFont(dpy, font);
	for (i = 0; i < num_screens; i++) {
		XFreeGC(dpy, screens[i].invert_gc);
		XInstallColormap(dpy, DefaultColormap(dpy, i));
	}
	free(screens);
	XCloseDisplay(dpy);
	exit(0);
}

int handle_xerror(Display *dsply, XErrorEvent *e) {
	(void)dsply;  /* unused */
	Client *c;

	if (ignore_xerror) {
		LOG_DEBUG("handle_xerror() ignored an XErrorEvent: %d\n", e->error_code);
		return 0;
	}
	/* If this error actually occurred while setting up the new
	 * window, best let make_new_client() know not to bother */
	if (initialising != None && e->resourceid == initialising) {
		LOG_DEBUG("\t **SAVED?** handle_xerror() caught error %d while initialising\n", e->error_code);
		initialising = None;
		return 0;
	}
	LOG_DEBUG("**ERK** handle_xerror() caught an XErrorEvent: error_code=%d request_code=%d minor_code=%d\n",
			e->error_code, e->request_code, e->minor_code);
	if (e->error_code == BadAccess && e->request_code == X_ChangeWindowAttributes) {
		LOG_ERROR("root window unavailable (maybe another wm is running?)\n");
		exit(1);
	}

	c = find_client(e->resourceid);
	if (c) {
		LOG_DEBUG("\thandle_xerror() : flagging client for removal\n");
		c->remove = 1;
		need_client_tidy = 1;
	}
	return 0;
}

void grab_keysym(Window w, unsigned int mask, KeySym keysym) {
	KeyCode keycode = XKeysymToKeycode(dpy, keysym);
	XGrabKey(dpy, keycode, mask, w, True,
			GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, keycode, mask|LockMask, w, True,
			GrabModeAsync, GrabModeAsync);
	if (numlockmask) {
		XGrabKey(dpy, keycode, mask|numlockmask, w, True,
				GrabModeAsync, GrabModeAsync);
		XGrabKey(dpy, keycode, mask|numlockmask|LockMask, w, True,
				GrabModeAsync, GrabModeAsync);
	}
}
