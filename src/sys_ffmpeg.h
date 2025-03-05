// -----------------------------------------------------------------------------
// ffmpeg video recording
// [src] http://blog.mmacklin.com/2013/06/11/real-time-video-capture-with-ffmpeg/

static FILE* rec_ffmpeg;
static FILE* rec_mpeg1;

int record_has_ffmpeg() {
#ifdef _WIN32
    // try to avoid flashing console in win32 app
    // this does not happen if launched from cmd.exe
    // or, if entrypoint is main() instead of WinMain()
    do_once {
        AllocConsole();
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    }
#endif
    // check if not installed or not in path or not executable or some other error
    static int has_ffmpeg;
    do_once has_ffmpeg = ifdef(win32, !system("where /Q ffmpeg"), !system("which ffmpeg"));
    return has_ffmpeg;
}

void record_stop(void) {
    if(rec_ffmpeg) ifdef(win32, _pclose, pclose)(rec_ffmpeg);
    rec_ffmpeg = 0;

    if(rec_mpeg1) fclose(rec_mpeg1);
    rec_mpeg1 = 0;
}

bool record_active() {
    return rec_ffmpeg || rec_mpeg1;
}

// returns the encoder used: 0 none, 1  jompeg, 2 ffmpeg
int record_start(const char *outfile_mp4, int w, int h, int fps) {
    do_once atexit(record_stop);

    record_stop();

    // first choice: external ffmpeg encoder
    if( !rec_ffmpeg ) {

        if (record_has_ffmpeg()) {
            char *cmd = va("ffmpeg%s "
                        "-hide_banner -loglevel error " // less verbose
                        "-r %d -f rawvideo -pix_fmt rgba -s %dx%d " // raw RGBA WxH-60Hz frames
                        // "-framerate 30 " // interpolating new video output frames from the source frames
                        "-i - "             // read frames from stdin
                        //"-draw_mouse 1 "
                        "-threads 0 "
                        //"-vsync vfr "
                        "-preset ultrafast " // collection of options that will provide a certain encoding speed [fast,ultrafast]
                        // "-tune zerolatency " // change settings based upon the specifics of your input
                        //"-crf 21 "           // range of the CRF scale [0(lossless)..23(default)..51(worst quality)]
                        "-pix_fmt yuv420p "  // compatible with Windows Media Player and Quicktime
    //                  "-vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2\" "
                        "-y \"%s\"", ifdef(win32, ".exe", ""),
                        fps, w, h, outfile_mp4);    // overwrite output file

            // -rtbufsize 100M (https://trac.ffmpeg.org/wiki/DirectShow#BufferingLatency) Prevent some frames in the buffer from being dropped.
            // -probesize 10M (https://www.ffmpeg.org/ffmpeg-formats.html#Format-Options) Set probing size in bytes, i.e. the size of the data to analyze to get stream information. A higher value will enable detecting more information in case it is dispersed into the stream, but will increase latency. Must be an integer not lesser than 32. It is 5000000 by default.
            // -c:v libx264 (https://www.ffmpeg.org/ffmpeg.html#Main-options) Select an encoder (when used before an output file) or a decoder (when used before an input file) for one or more streams. codec is the name of a decoder/encoder or a special value copy (output only) to indicate that the stream is not to be re-encoded.

            // open pipe to ffmpeg's stdin in binary write mode
            rec_ffmpeg = ifdef(win32, _popen(cmd, "wb"), popen(cmd, "w"));
        }
    }

    // fallback: built-in mpeg1 encoder
    if( !rec_ffmpeg ) {
        rec_mpeg1 = fopen(outfile_mp4, "wb"); // "a+b"
    }

    return rec_ffmpeg ? 2 : rec_mpeg1 ? 1 : 0;
}

void record_frame(void *pixels, int width, int height) {
    if( record_active() ) {
        if( rec_ffmpeg ) {
            fwrite(pixels, 4 * width * height, 1, rec_ffmpeg);
        }
        if( rec_mpeg1 ) {
            jo_write_mpeg(rec_mpeg1, pixels, width, height, 24);  // 24fps
        }
    }
}

