
/* The NOCURSES flag impacts compile time.
   Contrast with VTERM_FLAG_NOCURSES which does not impact the link
   dependencies but suppresses *use* of curses at runtime.
*/

#ifndef _VTERM_H_
#define _VTERM_H_

#include <inttypes.h>

#include <sys/types.h>

#ifdef NOCURSES
// short equivalent of the header, without needing the lib.
typedef unsigned long   chtype;
typedef unsigned char   bool;
typedef chtype          attr_t;

#undef TRUE
#define TRUE            1
#undef FALSE
#define FALSE           0

#define COLOR_BLACK     0
#define COLOR_RED       1
#define COLOR_GREEN     2
#define COLOR_YELLOW    3
#define COLOR_BLUE      4
#define COLOR_MAGENTA   5
#define COLOR_CYAN      6
#define COLOR_WHITE     7

#define COLOR_PAIR(n)  ((n) & 0xff)<<8

#define A_NORMAL        0x00000000
#define A_REVERSE       0x00040000
#define A_BLINK         0x00080000
#define A_BOLD          0x00200000
#define A_INVIS         0x00800000

#define NCURSES_ACS(x)  (((x)>0&&(x)<0x80)?((x)|0x00400000):0)

#define KEY_UP          259
#define KEY_DOWN        258
#define KEY_RIGHT       261
#define KEY_LEFT        260
#define KEY_BACKSPACE   263
#define KEY_IC          331
#define KEY_DC          330
#define KEY_HOME        262
#define KEY_END         360
#define KEY_PPAGE       339
#define KEY_NPAGE       338
#define KEY_SUSPEND     407

#define KEY_F(n)        ((n)>0 && (n)<50)?(n)+264:0

#else
#include <ncursesw/curses.h>
#endif

#define LIBVTERM_VERSION        "4.44"

#define VTERM_FLAG_RXVT         (1 << 0)    // masquerade as rxvt (default)
#define VTERM_FLAG_VT100        (1 << 1)    // masquerade as vt100
#define VTERM_FLAG_XTERM        (1 << 2)    // masquerade as xterm
#define VTERM_FLAG_XTERM_256    (1 << 3)    // masquerade as xterm-256
#define VTERM_FLAG_NOPTY        (1 << 8)    // skip all the fd and pty stuff.
                                            // just render input args byte
                                            // stream to a buffer

#define VTERM_FLAG_NOCURSES     (1 << 9)    // skip the curses WINDOW stuff.
                                            // return the char cell array if
                                            // required

#define VTERM_FLAG_DUMP         (1 << 10)    // tell libvterm to write
                                            // stream data to a dump file
                                            // for debugging


#define VCELL_TYPE_SIMPLE       0
#define VCELL_TYPE_WIDE         1

/*
    Need this to be public if we want to expose the buffer to callers.

    The ncurses implementation of cchar_t internally packs an attr_t.
    However, the only portable way to access that is via getcchar()
    and setcchar() which are a bit clumsy in their operation.
*/
struct _vterm_cell_s
{
    attr_t          attr;
    int             colors;
    short           fg;
    short           bg;
    short           f_rgb[3];
    short           b_rgb[3];

    wchar_t         wch[2];     // we need this when NOCURSES is defined

#ifndef NOCURSES
    cchar_t         uch;
#endif
};

typedef struct _vterm_cell_s    vterm_cell_t;

typedef struct _vterm_s         vterm_t;

typedef short (*VtermPairSelect) \
                    (vterm_t *vterm, short fg, short bg);

typedef int (*VtermPairSplit) \
                    (vterm_t *vterm, short pair, short *fg, short *bg);

typedef void (*VtermEventHook) \
                    (vterm_t *vterm, int event, void *anything);

/*
    certain events will trigger a callback if it's installed.  the
    callback "hook" is installed via the vterm_install_hook() API.

    the callback is invoked by the library, it supplies 3 arguements...
    a pointer to the vterm object, the event (an integer id), and
    an 'anything' data pointer.  the contents of 'anything' are
    dictated by the type of event and described as following:

    Event                           void *anything
    -----                           --------------
    VTERM_HOOK_BUFFER_ACTIVATED     index of buffer as int*
    VTERM_HOOK_BUFFER_DEACTIVATED   index of buffer as int*
    VTERM_HOOK_BUFFER_PREFLIP       index of new buffer as int*
    VTERM_HOOK_PIPE_READ            bytes read as ssize_t*
    VTERM_HOOK_PIPE_WRITTEN         unused
    VTERM_HOOK_TERM_PRESIZE         unused
    VTERM_HOOK_TERM_RESIZED         size as struct winsize*
    VTERM_HOOK_TERM_PRECLEAR        unused
*/

enum
{
    VTERM_HOOK_BUFFER_ACTIVATED     =   0x10,
    VTERM_HOOK_BUFFER_DEACTIVATED,
    VTERM_HOOK_BUFFER_PREFLIP,
    VTERM_HOOK_PIPE_READ,
    VTERM_HOOK_PIPE_WRITTEN,
    VTERM_HOOK_TERM_PRESIZE,
    VTERM_HOOK_TERM_RESIZED,
    VTERM_HOOK_TERM_PRECLEAR,
};


/*
    alloc a raw terminal object.

    @return:        a handle to a alloc'd vterm object.
*/
vterm_t*        vterm_alloc(void);

/*
    init a terminal object with a known height and width and alloc object
    if need be.

    @params:
        vterm       handle an already alloc'd vterm object
        width       desired width
        height      desired height
        flags       defined above as VTERM_FLAG_*

    @return:        a handle to a newly alloc'd vterm object if not provided.
*/
vterm_t*        vterm_init(vterm_t *vterm, uint16_t width, uint16_t height,
                    uint16_t flags);

/*
    convenience macro for alloc-ing a ready to use terminal object.

    @params:
        width       desired width
        height      desired height
        flags       defined above as VTERM_FLAG_*

    @return:        a handle to a newly alloc'd vterm object initialized using
                    the specified values.
*/
#define         vterm_create(width, height, flags) \
                    vterm_init(NULL, width, height, flags)

/*
    it's impossible for vterm to have insight into the color map because it
    is setup by the caller outside of the library.  therefore, the default
    function does an expensive iteration through all of the color pairs
    looking for a match everytime it's needed.  the caller probably knows
    a better way so this interface allows a different function to be set.

    @params:
        vterm       handle an already alloc'd vterm object
        ps          a callback which returns the index of a defined
                    color pair.

*/
void            vterm_set_pair_selector(vterm_t *vterm, VtermPairSelect ps);

/*
    returns a callback interface of the current color pair selector.  the
    default is basically a wrapper which leverages pair_content().

    @params:
        vterm       handle an already alloc'd vterm object

    @return:        a function pointer of the type VtermPairSelect
*/
VtermPairSelect vterm_get_pair_selector(vterm_t *vterm);

/*
    see note on vterm_set_pair_selector for rationale behind this API.
    this inteface provides the caller an oppurtunity to provide a more
    efficient means of unpacking a color index into it's respective
    foreground and background colors.

    @params:
        vterm       handle an already alloc'd vterm object
        ps          a callback which splits a color pair into its
                    respective foreground and background colorsl
*/
void            vterm_set_pair_splitter(vterm_t *vterm, VtermPairSplit ps);

/*
    returns a callback interface of the current color pair splitter.  the
    default is basically a wrapper for pair_content().

    @params:
        vterm       handle an already alloc'd vterm object

    @return:        a function pointer of the type VtermPairSplit
*/
VtermPairSplit  vterm_get_pair_splitter(vterm_t *vterm);

/*
    destroy a terminal object.

    @params:
        vterm       a valid vterm object handle.
*/
void            vterm_destroy(vterm_t *vterm);

/*
    fetch the process id (pid) of the terminal.

    @params:
        vterm       a valid vterm object handle.

    @return:        the process id of the terminal.
*/
pid_t           vterm_get_pid(vterm_t *vterm);

/*
    get the file descriptor of the pty.

    @params:
        vterm       a valid vterm object handle.

    @return:        the file descriptor of the terminal.
*/
int             vterm_get_pty_fd(vterm_t *vterm);

/*
    get the name of the tty.

    @params:
        vterm       a valid vterm object handle.

    @return:        the name of the tty.  it should be copied elsewhere
                    if intended to be used longterm.
*/
const char*     vterm_get_ttyname(vterm_t *vterm);

/*
    get the name of the window title (if set by OSC command)

    @params:
        vterm       a valid vterm object handle.
        buf         a buffer provided by the caller which the window
                    title will be copied into.
        buf_sz      the size of the caller provided buffer.  the
                    usable space will be buf_sz - 1 for null termination.
*/
void            vterm_get_title(vterm_t *vterm, char *buf, int buf_sz);

/*
    set a binary and args to launch instead of a shell.

    @params:
        vterm       a valid vterm object handle.
        path        the complete path to the binary (including the binary
                    itself.
        argv        a null-terminated vector of arguments to pass to the
                    binary.
*/
void            vterm_set_exec(vterm_t *vterm, char *path, char **argv);

/*
    read bytes from the terminal.

    @params:
        vterm       a valid vterm object handle.

    @return:        the amount of bytes read from running program output.
                    this is somewhat analogous to stdout.  returns -1
                    on error.
*/
ssize_t         vterm_read_pipe(vterm_t *vterm);

/*
    write a keystroke to the terminal.

    @params:
        vterm       a valid vterm object handle.

    @return:        returns 0 on success and -1 on error.
*/
int             vterm_write_pipe(vterm_t *vterm, uint32_t keycode);

/*
    installs an event hook.

    @params:
        vterm       a valid vterm object handle.
        hook        a callback that will be invoked by certain events.
*/
void            vterm_install_hook(vterm_t *vterm, VtermEventHook hook);

#ifndef NOCURSES
/*
    set the WINDOW * to that the terminal will use for output.

    @params:
        vterm       a valid vterm object handle.
        window      a ncurses WINDOW to use as a rendering surface
*/
void            vterm_wnd_set(vterm_t *vterm, WINDOW *window);

/*
    get the WINDOW* that the terminal is using.

    @params:
        vterm       a valid vterm object handle.

    @return:        a pointer to the WINDOW which is set as a rendering
                    surface.  returns null if none is set.
*/
WINDOW*         vterm_wnd_get(vterm_t *vterm);

/*
    cause updates to the terminal to be rendered

    @params:
        vterm       a valid vterm object handle.
*/
void            vterm_wnd_update(vterm_t *vterm);
#endif

/*
    set the foreground and bakground colors that will be used by
    default on erase operations.

    @params:
        vterm       a valid vterm object handle.
        fg          the default foreground color for the terminal.
        bg          the default background color for the terminal.

    @return:        returns 0 on success and -1 upon error.
*/
int             vterm_set_colors(vterm_t *vterm, short fg, short bg);

/*
    get the color pair number of the default fg/bg combination.

    @params:
        vterm       a valid vterm object handle.

    @return:        returns the color pair index set as the default
                    fg/bg color combination.  returns -1 upon error.
*/
long            vterm_get_colors(vterm_t *vterm);

/*
    erase the contents of the terminal.

    @params:
        vterm       handle to a vterm object
        idx         index of the buffer or -1 for current
*/
void            vterm_erase(vterm_t *vterm, int idx);

/*
    erase the specified row of the terminal.

    @params:
        vterm       handle to a vterm object
        row         zero-based index of the row to delete.  specifying
                    a value of -1 indicates current row.
*/
void            vterm_erase_row(vterm_t *vterm, int row);

/*
    erase the terminal beginning at a certain row and toward the bottom
    margin.

    @params:
        vterm       handle to a vterm object
        start_row   zero-based index of the row and subsequent rows below
                    to erase.
*/
void            vterm_erase_rows(vterm_t *vterm, int start_row);

/*
    erase the specified column of the terminal.

    @params:
        vterm       handle to a vterm object
        col         zero-based index of the column to delete.  specifying
                    a value of -1 indicates current column.
*/
void            vterm_erase_col(vterm_t *vterm, int col);

/*
    erase the terminal at a specific column and toward the right margin.

    @params:
        vterm       handle to a vterm object
        start_col   zero-based index of the column and subsequent columns
                    to the right to erase.
*/
void            vterm_erase_cols(vterm_t *vterm, int start_col);

/*
    cause the terminal to be scrolled up by one row and placing an empty
    row at the bottom.

    @params:
        vterm       handle to a vterm object
*/
void            vterm_scroll_up(vterm_t *vterm);

/*
    cause the termianl to be scrolled down by one row and placing an
    empty row at the top.

    @params:
        vterm       handle to a vterm object
*/
void            vterm_scroll_down(vterm_t *vterm);

/*
    this is a convenience macro to keep original behavior intact for
    applications that just don't care.  it assumes resize occurs from
    the bottom right origin.

    @params:
        vterm       handle to a vterm object
        width       new width of terminal to resize to
        height      new height of terminal to resize to
*/
#define         vterm_resize(vterm, width, height)  \
                    vterm_resize_full(vterm, width, height, 0, 0, 1, 1)

/*
    resize the terminal to a specific size.  when shrinking the terminal,
    excess will be clipped and discarded based on where the grip edges
    are originating from.

    this API is typically used in reponse to a SIGWINCH signal.

    @params:
        vterm       handle to a vterm object
        width       new width of terminal to resize to
        height      new height of terminal to resize to
        grip_top    unused
        grip_left   unused
        grip_right  unused
        grip_bottom unused
*/
void            vterm_resize_full(vterm_t *vterm,
                    uint16_t width, uint16_t height,
                    int grip_top, int grip_left,
                    int grip_right, int grip_bottom);

/*
    pushes new data into the interpreter.  if a WINDOW has been
    associated with the vterm object, the WINDOW contents are also
    updated.

    @params:
        vterm       handle to a vterm object
        data        data to push throug the intepreter.  typically this
                    comes from vterm_read_pipe().
        len         the length of the data being pushed into the
                    intepreter.
*/
void            vterm_render(vterm_t *vterm, const char *data, int len);

/*
    fetches the width and height of the current terminal dimentions.

    @params:
        vterm       handle to a vterm object
        width       a pointer to an integer where the width will be stored
        height      a pointer to an integer where the height will be stored
*/
void            vterm_get_size(vterm_t *vterm, int *width, int *height);

/*
    returns a copy of the active buffer screen matrix.

    @params:
        vterm       handle to a vterm object
        rows        integer pointer indicating the row count of the matrix
        cols        integer pointer indicating the column count of the matrix

*/
vterm_cell_t**  vterm_copy_buffer(vterm_t *vterm, int *rows, int *cols);

#endif

