
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <signal.h>

#include <sys/ioctl.h>

#include "vterm.h"
#include "vterm_private.h"
#include "vterm_misc.h"
#include "vterm_buffer.h"

void
vterm_resize_full(vterm_t *vterm, uint16_t width, uint16_t height,
                    int grip_top, int grip_left,
                    int grip_bottom, int grip_right)
{
    vterm_desc_t    *v_desc = NULL;
    struct winsize  ws = {.ws_xpixel = 0,.ws_ypixel = 0};
    int             delta_x;
    int             delta_y;
    int             start_x;
    int             start_y;
    int             idx;

    // suppress warnings
    (void)grip_top;
    (void)grip_left;
    (void)grip_bottom;
    (void)grip_right;

    if(vterm == NULL) return;
    if(width == 0 || height == 0) return;

    // set the vterm description buffer selector
    idx = vterm_buffer_get_active(vterm);
    v_desc = &vterm->vterm_desc[idx];

    delta_x = width - v_desc->cols;
    delta_y = height - v_desc->rows;
    start_x = v_desc->cols;
    start_y = v_desc->rows;

    // fire off the TERM RESIZED event hook if it's been set
    if(vterm->event_hook != NULL)
    {
        vterm->event_hook(vterm,
            VTERM_HOOK_TERM_PRESIZE, (void *)NULL);
    }

    // realloc to accomodate the new matrix size
    vterm_buffer_realloc(vterm, idx, width, height);

    if(!(v_desc->buffer_state & STATE_SCROLL_SHORT))
    {
        v_desc->scroll_max = height - 1;
    }

    ws.ws_row = height;
    ws.ws_col = width;

    clamp_cursor_to_bounds(vterm);

    if(delta_x > 0) vterm_erase_cols(vterm, start_x);
    if(delta_y > 0) vterm_erase_rows(vterm, start_y);

    // signal the child process that terminal changed size
    if(!(vterm->flags & VTERM_FLAG_NOPTY))
    {
        ioctl(vterm->pty_fd, TIOCSWINSZ, &ws);
        kill(vterm->child_pid, SIGWINCH);
    }

    // fire off the TERM RESIZED event hook if it's been set
    if(vterm->event_hook != NULL)
    {
        vterm->event_hook(vterm,
            VTERM_HOOK_TERM_RESIZED, (void *)&ws);
    }

    return;
}
