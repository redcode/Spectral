#define SPECTRAL "v0.3"

// # build (windows)
// cl run.c /O2 /MT /DNDEBUG /GL /GF /arch:AVX2
//
// # build (linux)
// sudo apt-get install mesa-common-dev libx11-dev gcc libgl1-mesa-dev
// gcc run.c -Wno-unused-result -Wno-format -lm -ldl -lX11 -lpthread -lGL -O3
//
// # usage
// - esc: game browser
// - f1: speed boost (hold)
// - f2: start/stop tape
// - f3/f4: rewind/advance tape
// - f5: restart
// - f6: toggle input latency (runahead)
// - f7: toggle tape polarity
// - f8: toggle tape speed
// - f9: toggle rf/crt
// - f11/f12: quick save/load 
// - alt+enter: fullscreen
// - tab+cursors: joysticks
//
// ref: http://www.zxdesign.info/vidparam.shtml
// https://worldofspectrum.org/faq/reference/48kreference.htm
// https://faqwiki.zxnet.co.uk/wiki/ULAplus
// https://foro.speccy.org/viewtopic.php?t=2319
// test(48): RANDOMIZE USR 46578
// test(48): RANDOMIZE USR 5050
// test(2A): RANDOMIZE USR 20000

//
// done
// cpu, ula, mem, rom, 48/128, key, joy, ula+, tap, ay, beep, sna/128, fps, tzx, if2, zip, rf, menu, kms, z80, scr,
// key2/3, +2a/+3, fdc, dsk,

// fix
// tzx(flow,gdb)
// kms windowed (clip), window focus @ MMB+tigr
// z80 or fdc not being reset sometimes: load mercs.dsk, then wrestlingSuperstars.dsk
//
// fixme: exolon main tune requires contended memory, otherwise it's too fast +15% tempo
// fixme: mouse clip when resizing app while mouse is hidden

// @todo:
// auto rewind
// disable mic_on if...
// input keyboard != space
// [ ] stop tape if next block is PROGRAM (side 2)
// [ ] stop tape if pause && key/joy pressed
// animated states

// @todo: tests
// - send keys via cmdline: "--keys 1,wait,wait,2"
// - send termination time "--maxidle 300"

// fixme: edos tapes: streecreedfootball(edos).tzx,
//      beyondtheicepalace(edos).tzx,elvenwarrior(edos).tzx, uses additional non-standard padding bytes in headers (19->29), making the EAR routine to loop pretty badly
// [x] LOAD "" CODE if 1st block is code
// [x] glue consecutive tzx/taps in zips (side A) -> side 1 etc)
// [ ] glue consecutive tzx/taps in disk
// [ ] glue: do not glue two consecutive tapes if size && hash match (both sides are same)
// [ ] glue: if tape1 does not start with a basic block, swap tapes
// [ ] scan programs.bas for "128" string on them. reposition marker on demand. (barbarian 2; dragon ninja)
// TS   / if in(fe) slows down -> pause tape for real (until in(fe) boosts up)
// prev \ stop tape -> slow down --> test joeblade2,atlantis,vegasolaris,amc
// score128: 128 in filename + memcmp("19XY") X <= 8 Y < 5 + sizeof(tape) + memfind("in ayffe") + side b > 48k + program name "128" + filename128  -> load as 128, anything else: load as usr0
//      if single bank > 49k (navy seals), if size(tap)>128k multiload (outrun)

// try
// https://github.com/anotherlin/z80emu
// https://github.com/kspalaiologos/tinyz80/

// try
// https://damieng.com/blog/2020/05/02/pokes-for-spectrum/

// try (verify dsk protections)
// https://simonowen.com/samdisk/sys_plus3/
// https://simonowen.com/blog/2009/03/21/further-edsk-extensions/
// https://simonowen.com/misc/extextdsk.txt
// http://www.cpctech.org.uk/docs/extdsk.html

// try (pentagon-128)
// https://worldofspectrum.net/rusfaq/index.html

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>

enum { _320 = 352, _319 = _320-1, _32 = (_320-256)/2 };
enum { _240 = 288, _239 = _240-1, _24 = (_240-192)/2 };

#include "3rd.h"
#include "emu.h"
#include "sys.h"
#include "zx.h"

// @fixme: ZX_RUNAHEAD: +3 are not compatible. AYUMI not compatible (see TargetRenegade128K).
// https://near.sh/articles/input/run-ahead https://www.youtube.com/watch?v=_qys9sdzJKI // https://docs.libretro.com/guides/runahead/

int ZX_RUNAHEAD = 0;

int file_is_supported(const char *filename) {
    // @todo: trd,scl
    const char *ext = strrchr(filename ? filename : "", '.');
    if( ext && strstr(".pok.sna.z80.tap.tzx.rom.dsk.scr.zip", ext) ) return 1;
    if( ext && strstr(".POK.SNA.Z80.TAP.TZX.ROM.DSK.SCR.ZIP", ext) ) return 1;
    return 0;
}

void regs(const char *title) {
    printf("\n--- %s ---\n", title);
    printf("af:%04x,af'%04x,bc:%04x,bc':%04x\n", AF(cpu), AF2(cpu), BC(cpu), BC2(cpu));
    printf("de:%04x,de'%04x,hl:%04x,hl':%04x\n", DE(cpu), DE2(cpu), HL(cpu), HL2(cpu));
    printf("iff%04x,im:%04x,ir:%02x%02x,ix :%04x,iy:%04x\n", IFF1(cpu) << 8 | IFF2(cpu), IM(cpu), I(cpu),R(cpu), IX(cpu), IY(cpu));
    printf("pc:%04x,sp:%04x\n", PC(cpu),SP(cpu));
    printf("out254(%02x) page128(%02x) page2a(%02x)\n", ZXBorderColor, page128, page2a);
    for( int i = 0; i < 16; ++i ) printf("%s%s%04x", i == 0 ? "ay " : " ", i == ay_current_reg ? "*":"", ay_registers[i]); puts("");
    printf("mem%d%d%d%d%s ", !!(page128&16), (page128&8?7:5), 2, page128&7, page128&32?"!":"");
    for( int i = 0; i < 16; ++i ) printf("%s%04x", i == 0 ? "" : " ", (crc32(0,RAM_BANK(i), 0x4000) & 0xffff) ^ 0xd286); puts("");
}

int trapped;
void tape_hooks() {
    return;

    if( IFF1(cpu) ) return; // int pending

    if( ZX > 128 ) return;
    if( ZX == 128 ) if( !(page128 & 16) ) return;

    unsigned pc = PC(cpu);

    if (pc == 0x0556) {
        puts("trap on!");
//        trapped = 1;
        mic_on = 1;
    }

    if (/*trapped &&*/ (pc < 0x0556 || pc >= 0x0605)) {
        trapped = 0;
        mic_on = 0;
        puts("trap off!");
    }
}


static int flick_frame = 0;
static int flick_hz = 0;

void draw(window *win, int y /*0..311 tv scanline*/) {
    int width = _32+256+_32;
    rgba *texture = &((rgba*)win->pix)[0 + y * width];

    // int third = y / 64; int y64 = y % 64;
    // int bit3swap = (y64 & 0x38) >> 3 | (y64 & 0x07) << 3;
    // int scanline = (bit3swap + third * 64) << 5;
    #define SCANLINE(y) \
        ((((((y)%64) & 0x38) >> 3 | (((y)%64) & 0x07) << 3) + ((y)/64) * 64) << 5)

    // border left
    for(int x=0;x<_32;++x) *texture++=ZXPalette[ZXBorderColor];
    // screen
    if( !(y >= (0+_24) && y < (192+_24) )) {
    for(int x=0;x<256;++x) *texture++=ZXPalette[ZXBorderColor];
    } else {
        y -= _24;
        byte *pixels=VRAM+SCANLINE(y);
        byte *attribs=VRAM+6144+((y&0xF8)<<2);

*texture = ZXPalette[ZXBorderColor];
int shift0 = ZX_RF ? (rand()<(RAND_MAX/256)) : 0; // flick_frame * -(!!((y+0)&0x18))
texture += shift0;

        for(int x = 0; x < 32; ++x) {
            byte attr = *attribs;
            byte pixel = *pixels, fg, bg;

            // @fixme: make section branchless

            if (ulaplus_enabled) {
                fg = ((attr & 0xc0) >> 2) | ((attr & 0x07));
                bg = ((attr & 0xc0) >> 2) | ((attr & 0x38) >> 3) | 8;
            } else {
                pixel ^= (attr & 0x80) && ZXFlashFlag ? 0xff : 0x00;
                fg = (attr & 0x07) | ((attr & 0x40) >> 3);
                bg = (attr & 0x78) >> 3;
            }

            // @fixme: make section branchless

            texture[0]=ZXPalette[pixel & 0x80 ? fg : bg];
            texture[1]=ZXPalette[pixel & 0x40 ? fg : bg];
            texture[2]=ZXPalette[pixel & 0x20 ? fg : bg];
            texture[3]=ZXPalette[pixel & 0x10 ? fg : bg];
            texture[4]=ZXPalette[pixel & 0x08 ? fg : bg];
            texture[5]=ZXPalette[pixel & 0x04 ? fg : bg];
            texture[6]=ZXPalette[pixel & 0x02 ? fg : bg];
            texture[7]=ZXPalette[pixel & 0x01 ? fg : bg];

            texture += 8;

            pixels++;
            attribs++;
        }

texture -= shift0;

    }
    // border right
    for(int x=0;x<_32;++x) *texture++=ZXPalette[ZXBorderColor];

#ifdef DEBUG_SCANLINE
    memset(texture, 0xFF, width * 4);
    //    sys_sleep(1000/60.);
#endif
}

void blur(window *win) {
    int height = _24+192+_24;
    int width = _32+256+_32;

    // screen
    static int j = 0;
    static byte jj = 0;

    for( int y = _24; y < _240-_24; ++y ) {
        rgba *texture = &((rgba*)win->pix)[_32 + y * width];

        for(int x=0;x<_32;x++) {

        int shift = (++jj)&1;
        texture += shift;

                for(int i = 8-1; i >= 0; --i) {
                    unsigned pix0 = texture[i-0];
                    unsigned pix1 = texture[i-1];
                    unsigned pix2 = texture[i-2];
                    unsigned r0,g0,b0; rgb_split(pix0,r0,g0,b0);
                    unsigned r1,g1,b1; rgb_split(pix1,r1,g1,b1);
                    unsigned r2,g2,b2; rgb_split(pix2,r2,g2,b2);
                    if(i&1)
                    texture[i] = rgb((r0+r0+r0+r0)/4,(g0+g0+g0+g0)/4,(b2+b2+b2)/3); // yellow left
                    else
                    texture[i] = rgb((r0+r0+r0+r1)/4,(g0+g0+g0+g1)/4,(b1+b1+b1+b2)/4); // blue left
                    //else
                    //texture[i] = rgb((r0+r1+r1+r1)/4,(g0+g1+g1+g1)/4,(b0+b1+b1)/3);

if(ZX_RF) {
                    continue;
                    // saturate aberrations (very slow)
                    pix0 = texture[i];
                    rgb_split(pix0,r0,g0,b0);
                    byte h0,s0,v0; rgb2hsv(r0,g0,b0,&h0,&s0,&v0);
                    if( s0 * 1.5 > 255 ) s0 = 255; else s0 *= 1.5;
                    if( v0 * 1.01 > 255 ) v0 = 255; else v0 *= 1.01;
                    texture[i] = as_rgb(h0,s0,v0);
}
                }
            // interesting tv effects (j): 13, 19, 23, 27, 29, 33

                if(x%2)
                for(int i = 0; i < 8; ++i) {
                    unsigned pix0 = texture[i-0];
                    byte r0,g0,b0; rgb_split(pix0,r0,g0,b0);
                    byte h0,s0,v0; rgb2hsv(r0,g0,b0,&h0,&s0,&v0);
                    texture[i] = as_rgb(h0,s0,v0*0.99);
                }

                for(int i = 0; i < 8; ++i) { ++j; j%=33;
                    unsigned pix0 = texture[i-0];
                    byte r0,g0,b0; rgb_split(pix0,r0,g0,b0);
                    byte h0,s0,v0; rgb2hsv(r0,g0,b0,&h0,&s0,&v0);
                    if(j<1) texture[i] = as_rgb(h0,s0,v0*0.95);
                    else
                    if(j<2) texture[i] = as_rgb(h0,s0,v0*0.95);
                    else
                            texture[i] = rgb(r0,g0,b0);
                }

        texture -= shift;

            texture += 8;
        }
    }
}

void scanlines(window *win) {
    // hsv: slow. may be better in a shader
    int height = _24+192+_24;
    int width = _32+256+_32;
    for(int y = 0; y < _240; y+=2) {
        rgba *texture = &((rgba*)win->pix)[0 + y * width];
        for(int x = 0; x < width; ++x) {
            unsigned pix0 = texture[x];
            byte r0,g0,b0; rgb_split(pix0,r0,g0,b0);
            byte h0,s0,v0; rgb2hsv(r0,g0,b0,&h0,&s0,&v0);
            texture[x] = as_rgb(h0*0.99,s0,v0*0.98);
        }
    }
}

const char *shader = 
#if 0
"/* HSV from/to RGB conversion functions by Inigo Quilez. https://www.shadertoy.com/view/lsS3Wc (MIT licensed)*/\n"
"const float eps = 0.0000001;\n"
"vec3 hsv2rgb( vec3 c ) {\n"
"    vec3 rgb = clamp( abs(mod(c.x*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0, 1.0 );\n"
"    return c.z * mix( vec3(1.0), rgb, c.y);\n"
"}\n"
"vec3 rgb2hsv( vec3 c) {\n"
"    vec4 k = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);\n"
"    vec4 p = mix(vec4(c.zy, k.wz), vec4(c.yz, k.xy), (c.z<c.y) ? 1.0 : 0.0);\n"
"    vec4 q = mix(vec4(p.xyw, c.x), vec4(c.x, p.yzx), (p.x<c.x) ? 1.0 : 0.0);\n"
"    float d = q.x - min(q.w, q.y);\n"
"    return vec3(abs(q.z + (q.w - q.y) / (6.0*d+eps)), d / (q.x+eps), q.x);\n"
"}\n"

"/* YUV conversions by icalvin102 https://www.shadertoy.com/view/3lycWz */\n"
"vec3 rgb2yuv(vec3 rgb){\n"
"    float y = 0.299*rgb.r + 0.587*rgb.g + 0.114*rgb.b;\n"
"    return vec3(y, 0.493*(rgb.b-y), 0.877*(rgb.r-y));\n"
"}\n"
"vec3 yuv2rgb(vec3 yuv){\n"
"    float y = yuv.x;\n"
"    float u = yuv.y;\n"
"    float v = yuv.z;   \n"
"    return vec3(\n"
"        y + 1.0/0.877*v,\n"
"        y - 0.39393*u - 0.58081*v,\n"
"        y + 1.0/0.493*u\n"
"    );\n"
"}\n"
#endif
    "/* based on code by lalaoopybee https://www.shadertoy.com/view/DlfSz8 */\n"
    "#define CURVATURE 8.2\n"
    "#define BLUR .01\n"
    "#define CA_AMT 1.0024\n"
    "void fxShader(out vec4 fragColor, in vec2 uv){\n"
    "    vec2 fragCoord=uv*vec2(352.0*3.0,288.0*3.0);\n"
#if 1
    "    /* curvature */\n"
    "    vec2 crtUV=uv*2.-1.;\n"
    "    vec2 offset=crtUV.yx/CURVATURE;\n"
    "    crtUV+=crtUV*offset*offset;\n"
    "    crtUV=crtUV*.5+.5;\n"

    "    /* edge blur */\n"
    "    vec2 edge=smoothstep(0., BLUR, crtUV)*(1.-smoothstep(1.-BLUR, 1., crtUV));\n"
#else
    "    vec2 crtUV = uv;\n"
    "    vec2 edge = vec2(1.,1.);\n"
#endif
#if 1
    "    /* chromatic aberration */\n"
    "    fragColor.rgb=vec3(\n"
    "        texture(image, (crtUV-.5)*CA_AMT+.5).r,\n"
    "        texture(image, crtUV).g,\n"
    "        texture(image, (crtUV-.5)/CA_AMT+.5).b\n"
    "    )*edge.x*edge.y;\n"
#else
    "    fragColor = texture(image, crtUV) * edge.x * edge.y;\n"
#endif
#if 0
    "    /* diodes */\n"
    "    if(mod(fragCoord.y, 2.)<1.) fragColor.rgb*=.95;\n"
    "    else if(mod(fragCoord.x, 3.)<1.) fragColor.rgb*=.95;\n"
    "    else fragColor*=1.05;\n"
#endif
    "}\n";

int load_shader(const char *filename) {
    char *data = readfile(filename, NULL);
    if(data) if(strstr(data, " fxShader")) return shader = strdup(data), 1; // @leak
    return 0;
}

#include <time.h>
int screenshot(const char *filename) {
    static byte counter = 0xFF; counter = (counter + 1) % 50;

    time_t timer = time(NULL);
    struct tm* tm_info = localtime(&timer);

    char stamp[32];
    strftime(stamp, 32, "%Y%m%d %H%M%S", tm_info);

    extern window* app;
    int ok1 = writefile(va("%s %s %04x.scr", filename, stamp, counter), VRAM, 6912);
    int ok2 = tigrSaveImage(va("%s %s %04x.png", filename, stamp, counter), app);

    return ok1 && ok2;
}



static byte* merge = 0;
static size_t merge_len;
void glue_init() {
    free(merge); merge = 0;
    merge_len = 0;
}
void *glue(const byte *data, size_t len) {
    merge = realloc(merge, merge_len + len);
    memcpy(merge + merge_len, data, len);
    merge_len += len;
    return merge;
}
size_t glue_len() {
    return merge_len;
}


void *zip_read(const char *filename, size_t *size) {
    glue_init();

    // up to 64 tapes per zip (usually up to 4 max)
    char *alpha_tapes[64] = {0};
    int count_tapes = 0;

    // test contents of file
    zip *z = zip_open(filename, "rb");
    if( z ) {
        for( unsigned i = 0 ; count_tapes < 64 && i < zip_count(z); ++i ) {
            void *data = file_is_supported(zip_name(z,i)) ? zip_extract(z,i) : 0;
            if(!data) continue;
            free(data);

            alpha_tapes[count_tapes++] = zip_name(z,i);
        }

        if( count_tapes )
        qsort(alpha_tapes, count_tapes, sizeof(char *), qsort_strcmp);

        for( unsigned j = 0 ; j < count_tapes; ++j ) {
            int i = zip_find(z, alpha_tapes[j]); // convert entry to index. returns <0 if not found.

            printf("  %d) ", i+1);
            printf("[%08X] ", zip_hash(z,i));
            printf("$%02X ", zip_codec(z,i));
            printf("%s ", zip_modt(z,i));
            printf("%5s ", zip_file(z,i) ? "" : "<dir>");
            printf("%11u ", zip_size(z,i));
            printf("%s ", zip_name(z,i));
            void *data = zip_extract(z,i);
            printf("\r%c\n", data ? 'Y':'N'); // %.*s\n", zip_size(z,i), (char*)data);

            // append data to merge
            glue(data, zip_size(z,i));
            if(size) *size = merge_len;
            free(data);

            // keeping glueing tapes
            if(strstr(zip_name(z,i),".tap") || strstr(zip_name(z,i),".tzx")) {
            continue;
            }
            break;
        }

        zip_close(z);
        return merge;
    } else {
        printf("error: bad %s zip\n", filename);
    }
    return 0;
}



static char *last_load = 0;
int load(const char *file, int must_clear, int do_rompatch) {
    if( !file ) return 0;
    last_load = (free(last_load), strdup(file));

    if( must_clear ) {
        // hard reset
#if 0

#if NEWCORE
pins = z80_reset(&cpu);
#else
        z80_reset(&cpu);
#endif

#else
        // http://www.z80.info/interrup.htm
        IFF1(cpu) = IFF2(cpu) = IM(cpu) = 0;
        PC(cpu) = I(cpu) = R(cpu) = 0;
        SP(cpu) = AF(cpu) = 0xffff;
#endif

        // soft reset
        mic_reset();
        trapped = 0;
rom_patch(do_rompatch);
        /*page128 = 0;*/
        if(ZX>=128) port_0x7ffd(0); // 128
        if(ZX>=210) port_0x1ffd(0); // +2a/+3
    }

#if FLAGS & TESTS
    printf("\n\n%s\n-------------\n\n", file);
#endif

    char *ptr = 0; size_t size = 0;
    void *zip_read(const char *filename, size_t *size);
    if( strstr(file, ".zip") ) {
        ptr =  zip_read(file, &size);
        if(!(ptr && size))
            return 0;
    }

    if(!ptr)
    for( FILE *fp = fopen(file,"rb"); fp; fclose(fp), fp = 0) {
        fseek(fp, 0L, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        ptr = malloc(size);
        fread(ptr, 1, size, fp);
    }

    if(!(ptr && size))
        return 0;

    // dsk first
    if(!memcmp(ptr, "MV - CPC", 8) || !memcmp(ptr, "EXTENDED", 8)) {
        if(must_clear) z80_load(ldplus3, sizeof(ldplus3)), pins = z80_prefetch(&cpu, cpu.pc);
        return dsk_load(ptr, size), 1;
    }

    #define preload_snap(blob,len) do { \
        if( sna_load(blob,len) || z80_load(blob,len) ) { \
            /* clear vram in case pre-loaders are not clear snaps */ \
            /* set border, ink and paper according to current ROM theme */ \
            ZXBorderColor = VRAM[6144] & 7; \
            memset(VRAM,0,6144); \
            memset(VRAM+6144,VRAM[6144],32*24); \
        } } while(0)

    // pre-loaders
    const byte*    bins[] = { ld128bas, ld48bas, ld128bin, ld48bin };
    const unsigned lens[] = { sizeof(ld128bas), sizeof(ld48bas), sizeof(ld128bin), sizeof(ld48bin) };

    // tapes first
    if(tzx_load(ptr, (int)size)) {
        int is_bin = mic_data[1] == 3, choose = (ZX < 128) + is_bin * 2;
        if(must_clear) preload_snap(bins[choose], lens[choose]);
rom_patch(mic_queue_has_turbo ? 0 : do_rompatch);
pins = z80_prefetch(&cpu, cpu.pc);
        return 1;
    }
    if(tap_load(ptr,(int)size)) {
        int is_bin = mic_data[1] == 3, choose = (ZX < 128) + is_bin * 2;
        if(must_clear) preload_snap(bins[choose], lens[choose]);
rom_patch(do_rompatch); // beware: wont work for wizball.tap (custom loader)
pins = z80_prefetch(&cpu, cpu.pc);
        return 1;
    }

    // headerless fixed-size formats now, sorted by ascending file size.
    if( scr_load(ptr, size) ) {
pins = z80_prefetch(&cpu, cpu.pc);
        return 1;
    }
    if( rom_load(ptr, size) ) {
pins = z80_prefetch(&cpu, cpu.pc);
        return 1;
    }
    if( sna_load(ptr, size) ) {
pins = z80_prefetch(&cpu, cpu.pc);
        return /*regs("load"),*/ 1;
    }

    // headerless variable-size formats now
    if( *ptr == 'N' && pok_load(ptr, size) ) {
        return 1;
    }
    if( z80_load(ptr, size) ) {
pins = z80_prefetch(&cpu, cpu.pc);
        return /*regs("load"),*/ 1;
    }

    puts("unknown file format");
    return 0;
}

void crt(int enable) {
    extern window *app;
    if( enable )
    tigrSetPostShader(app, shader, strlen(shader));
    else
    tigrSetPostShader(app, tigr_default_fx_gl_fs, strlen(tigr_default_fx_gl_fs));
}




window *app, *ui;
int do_disasm = 0;
void input() {
    // keyboard
    ZXKeyUpdate();

    // joysticks
    int up   = window_pressed(app, TK_UP),   down = window_pressed(app, TK_DOWN);
    int left = window_pressed(app, TK_LEFT), right = window_pressed(app, TK_RIGHT);
    int fire = window_pressed(app, TK_TAB);
    ZXJoysticks(up,down,left,right,fire);

    #define KEYS(X) \
        X(0)X(1)X(2)X(3)X(4)X(5)X(6)X(7)X(8)X(9)\
        X(A)X(B)X(C)X(D)X(E)X(F)X(G)X(H)X(I)X(J)\
        X(K)X(L)X(M)X(N)X(O)X(P)X(Q)X(R)X(S)X(T)\
        X(U)X(V)X(W)X(X)X(Y)X(Z)
    #define X(x) if(window_pressed(app, 0[#x])) ZXKey(ZX_##x);
    KEYS(X);
    if(window_pressed(app, TK_SPACE))      {ZXKey(ZX_SPACE); /*if(mic_on) mic_on = 0, tap_prev();*/ }
    if(window_pressed(app, TK_BACKSPACE))  {ZXKey(ZX_SHIFT); ZXKey(ZX_0);}
    if(window_pressed(app, TK_RETURN))      ZXKey(ZX_ENTER);
    if(window_pressed(app, TK_SHIFT))       ZXKey(ZX_SHIFT);
    if(window_pressed(app, TK_CONTROL))     ZXKey(ZX_SYMB);
    if(window_pressed(app, TK_ALT))         ZXKey(ZX_CTRL);

#if FLAGS & DEV
    if(window_trigger(app, TK_F1))        boost_on ^= 1;
#else
    if(window_pressed(app, TK_F1))        boost_on = 1; else boost_on = 0;
#endif
    if(window_trigger(app, TK_F2))        mic_on ^= 1;
    if(window_trigger(app, TK_F3))        tap_prev();
    if(window_trigger(app, TK_F4))        tap_next();
    if(window_trigger(app, TK_F5))        { clear(window_pressed(app,TK_SHIFT) ? 48 : 128); load(last_load, 1, window_pressed(app,TK_CONTROL) ? 1:0); }
#if FLAGS & RUNAHEAD
    if(window_trigger(app, TK_F6))        ZX_RUNAHEAD ^= 1;
#endif
    if(window_trigger(app, TK_F7))        mic_invert_polarity ^= 1;
    if(window_trigger(app, TK_F8))        ZX_FAST ^= 1;
    if(window_trigger(app, TK_F9))        { static int mode = 0; do_once mode = ZX_CRT << 1 | ZX_RF; mode = (mode + 1) & 3; ZX_RF = mode & 1; crt( ZX_CRT = !!(mode & 2) ); }
    if(window_trigger(app, TK_F11))       checkpoint_save(0);
    if(window_trigger(app, TK_F12))       checkpoint_load(0);

    // screenshots
    static int prev = 0;
    int key = !!(GetAsyncKeyState(VK_SNAPSHOT) & 0x8000);
    if( key ^ prev ) {
        char buffer[128];
        GetWindowTextA((HWND)((app)->handle), buffer, 128);
        screenshot( buffer );
    }
    prev = key;
}


void frame(int drawmode) { // no render (<0), full screen (0), scanlines (1)

zx_vsync = 1;

// NO RENDER
if( drawmode < 0 ) {
    sim(ZX_TS);
    return;
}

// FRAME RENDER
if( drawmode == 0 ) {
    sim(ZX_TS);
        for( int y = 0; y < 24+192+24; ++y ) draw(app, y);
    return;
}

// https://zx-pk.ru/threads/7720-higgins-spectrum-emulator/page4.html
//
// https://wiki.speccy.org/cursos/ensamblador/interrupciones
// Tiempo (en t-estados) que tarda el haz en…  Modelos 16K y 48K   Modelos 128K, +2, +2A y +3
// Renderizar uno de los bordes laterales en un scanline (izquierdo o derecho) 24  26
// Renderizar un scanline (la parte gráfica “central” de la imagen)    128 128
// Volver desde el extremo derecho al inicio del borde del siguiente scanline  48  48
// Renderizar un scanline completo con borde y retorno 224 (24+128+24+48)  228
// Renderizar toda la pantalla visible (192 scanlines) 43008 (192*224) 43776 (192*228)
// Renderizar la parte superior del borde  14336 (224*64)  14364 (228*63)
// Alcanzar el pixel (0,0) de la imagen    14408 ((224*64)+48+24)  14666 ((228*64)+48+26)
// Renderizar el borde inferior (56 scanlines) 12544 (224*56)  12768 (228*56)
// Volver desde el parte inferior a la parte superior izquierda (8 scans)
// (por eso sólo dibuja 56 y no 64/63 scanlines en la parte inferior)  1792 (224*8)    1824 (228*8)
// Número de scanlines dibujados (contando bordes) 312 311
// Renderizar un frame completo (bordes, scanlines y retornos) 69888 (14336+12544+43008)   70908


// SCANLINE RENDER
if( drawmode == 1 ) {

    if( ZX < 128 ) {
        // 48K
        // After an interrupt occurs, 64 line times (14336 T states; see below for exact timings) pass before the first byte of the screen (16384)
        // is displayed. At least the last 48 of these are actual border-lines; the others may be either border or vertical retrace.
        // Then the 192 screen+border lines are displayed, followed by 56 border lines again. Note that this means that a frame is
        // (64+192+56)*224=312*(24+128+24+48)=69888 T states long,
        int TS = 224; // 224 TS/scanline, 312 scanlines (first one is 24 ts + 50 ts(int) + 150 ts)
        for( int y = 0; y <  64; ++y ) { sim(TS); if(y>(64-_24-1)) draw(app, y-(64-_24)); }
        for( int y = 0; y < 192; ++y ) { sim(TS);           draw(app, _24+y); }
        for( int y = 0; y <  56; ++y ) { sim(TS); if(y<_24) draw(app, _24+192+y); }
    } else {
        // 128K
        // There are 63 scanlines before the television picture, as opposed to 64. 64*228 ~= 14361
        // To modify the border at the position of the first byte of the screen (see the 48K ZX Spectrum section for details), the OUT must finish after 14365, 14366, 14367 or 14368 T states have passed since interrupt. As with the 48K machine, on some machines all timings (including contended memory timings) are one T state later.
        // Note that this means that there are 70908 T states per frame, and the '50 Hz' interrupt occurs at 50.01 Hz, as compared with 50.08 Hz on the 48K machine. The ULA bug which causes snow when I is set to point to contended memory still occurs, and also appears to crash the machine shortly after I is set to point to contended memory.
        // (63+192+56)*228=311*(26+128+26+48)=70908 T states long,
        int TS = 228; // 228 TS/scanline, 311 scanlines
        for( int y = 1; y <  64; ++y ) { sim(TS); if(y>(64-_24-1)) draw(app, y-(64-_24)); }
        for( int y = 0; y < 192; ++y ) { sim(TS);           draw(app, _24+y); }
        for( int y = 0; y <  56; ++y ) { sim(TS); if(y<_24) draw(app, _24+192+y); }
    }
}

#if 0
// PIXEL RENDER
if( drawmode == 2 ) {
    TS = 0;

    // 128K
    // There are 63 scanlines before the television picture, as opposed to 64.
    // To modify the border at the position of the first byte of the screen (see the 48K ZX Spectrum section for details), the OUT must finish after 14365, 14366, 14367 or 14368 T states have passed since interrupt. As with the 48K machine, on some machines all timings (including contended memory timings) are one T state later.
    // Note that this means that there are 70908 T states per frame, and the '50 Hz' interrupt occurs at 50.01 Hz, as compared with 50.08 Hz on the 48K machine. The ULA bug which causes snow when I is set to point to contended memory still occurs, and also appears to crash the machine shortly after I is set to point to contended memory.
    // 228 TS/scanline, 311 scanlines

    // A single ZX Spectrum display row takes 224 T-States, including the horizontal flyback. For every T-State 2 pixels are written to the display, so 128 T-States pass for the 256 pixels in a display row. The ZX Spectrum is clocked at 3.5 MHz, so if 2 pixels are written in a single CPU clock cycle, the pixel clock of our display must be 7 MHz. A single line thus takes 448 pixel clock cycles.
    // The left and right border areas can be shown to be 48 pixels wide, which gives a visible row of 352 pixels (and the border equivalent) in total. That would take 352 / 2 = 176 T-States to display, and we know that a display row takes 224 T-States, so the lost 96 T-States must be used during the horizontal flyback of the electron beam to the start of the new row.

    static int vs = 0;
    if(window_trigger(app,'T')) { vs+=1; printf("%d\n", vs); }
    if(window_trigger(app,'G')) { vs-=1; printf("%d\n", vs); }

    for( int tv = 0; tv < 311; ++tv ) {
        int y = (tv - 24);
        zx_vsync = (tv == vs);
        if(y < 0 || y >= 240) sim(228);
        else {
            // 16px left extra
            sim(2*4);
            // 32px left
            sim(4*4);
            draw(app, y, 0, 4);
            // 256px main
            int excess = 0;
            for( int x = 0; x < 32; ++x ) {
                sim(4 - excess);
                excess = 0;
                excess += !!vram_accesses * contended[TS/4];
                vram_accesses = 0;
                draw(app, y, 32+x*8, 1);
            }
            // 32px right
            sim(4*4);
            draw(app, y, 32+256, 4);
            // 16px right extra
            sim(2*4);
            // retrace
            sim(52);
        }
    }
}
#endif

#if FLAGS & RF
    // detect ZX_RF flip-flop
    static int ZX_RF_old = 1;
    int refresh = ZX_RF ^ ZX_RF_old;
    ZX_RF_old = ZX_RF;

    if(ZX_RF) {
        static window *blend = 0;
        if(!blend) blend = window_bitmap(_320, _240);

        // reset bitmap contents when re-enabling ZX_RF
        if( refresh ) tigrBlit(blend, app, 0,0, 0,0, _320,_240);

        // ghosting
        tigrBlitAlpha(blend, app, 0,0, 0,0, _320,_240, 0.5f); //0.15f);
        tigrBlit(app, blend, 0,0, 0,0, _320,_240);

        // scanlines
        scanlines(app);

        // aberrations
        flick_hz ^= 1; // &1; // (rand()<(RAND_MAX/255));
        flick_frame = cpu.r; // &1; // (rand()<(RAND_MAX/255));
        blur(app);
    }
#endif

}


char **games = 0;
int *dbgames = 0;
int numgames = 0;
int numok=0,numwarn=0,numerr=0; // stats
void rescan() {
    // clean up
    while( numgames ) free(games[--numgames]);
    free(games);

    // refresh stats
    {
        numok=0,numwarn=0,numerr=0;

        const char *folder = "./games/";
#if FLAGS & TESTS
        folder = "./src/tests/";
#endif
        for( dir *d = dir_open(folder, "r"); d; dir_close(d), d = NULL ) {
            for( unsigned count = 0, end = dir_count(d); count < end; ++count ) {
                if( !dir_file(d, count) ) continue;

                const char *fname = dir_name(d, count);
                if( strendi(fname, ".db") ) {
                    for(FILE *fp2 = fopen(fname, "rb"); fp2; fclose(fp2), fp2=0) {
                        int ch;
                        fscanf(fp2, "%d", &ch); ch &= 0xFF;
                        numok += ch == 1;
                        numerr += ch == 2;
                        numwarn += ch == 3;
                    }
                }
            }
            for( unsigned count = 0, end = dir_count(d); count < end; ++count ) {
                if( !dir_file(d, count) ) continue;

                const char *fname = dir_name(d, count);
                if( file_is_supported(fname) ) {
                    // append
                    ++numgames;
                    games = realloc(games, numgames * sizeof(char*) );
                    games[numgames-1] = strdup(fname);
                    //
                    dbgames = realloc(dbgames, numgames * sizeof(char*) );
                    dbgames[numgames-1] = db_get(fname);
                }
            }    
        }
    }

    printf("%d games\n", numgames);
}

int active = 0, selected = 0, scroll = 0;
int game_browser() { // returns true if loaded
    tigrClear(ui, !active ? tigrRGBA(0,0,0,0) : tigrRGBA(0,0,0,128));

    if( !numgames ) return 0;

    if( window_trigger(app, TK_ESCAPE) )         active ^= 1;
    if( window_trigger(app, TK_F2) && !mic_last_type[0] ) active ^= 1; // open browser if start_tape is requested but no tape has been ever inserted
    if( !active ) return 0;

//  tigrBlitTint(app, app, 0,0, 0,0, _320,_240, tigrRGB(128,128,128));

    enum { ENTRIES = (_240/11)-4 };
    static char *buffer = 0; if(!buffer) { buffer = malloc(65536); /*rescan();*/ }
    if (!numgames) return 0;
    if( scroll < 0 ) scroll = 0;
    for( int i = scroll; i < numgames && i < scroll+ENTRIES; ++i ) {
        const char starred = dbgames[i] >> 8 ? (char)(dbgames[i] >> 8) : ' ';
        sprintf(buffer, "%c %3d.%s%s\n", starred, i+1, i == selected ? " > ":" ", 1+strrchr(games[i], DIR_SEP) );
        window_printxycol(ui, buffer, 1, 4+(i-scroll-1),
            (dbgames[i] & 0x7F) == 0 ? tigrRGB(255,255,255) : // untested
            (dbgames[i] & 0x7F) == 1 ? tigrRGB(64,255,64) :   // ok
            (dbgames[i] & 0x7F) == 2 ? tigrRGB(255,64,64) : tigrRGB(255,192,64) ); // bug:warn
    }

    int up = 0, pg = 0;

    static int UPcnt = 0; UPcnt *= !!window_pressed(app, TK_UP);
    if( window_pressed(app, TK_UP) && (UPcnt++ == 0 || UPcnt > 32) ) {
        up = -1;
    } UPcnt *= !!window_pressed(app, TK_UP);
    static int DNcnt = 0; DNcnt *= !!window_pressed(app, TK_DOWN);
    if( window_pressed(app, TK_DOWN) && (DNcnt++ == 0 || DNcnt > 32) ) {
        up = +1;
    } DNcnt *= !!window_pressed(app, TK_DOWN);
    static int PGUPcnt = 0; PGUPcnt *= !!window_pressed(app, TK_PAGEUP);
    if( window_pressed(app, TK_PAGEUP) && (PGUPcnt++ == 0 || PGUPcnt > 32) ) {
        pg = -1;
    } PGUPcnt *= !!window_pressed(app, TK_PAGEUP);
    static int PGDNcnt = 0; PGDNcnt *= !!window_pressed(app, TK_PAGEDN);
    if( window_pressed(app, TK_PAGEDN) && (PGDNcnt++ == 0 || PGDNcnt > 32) ) {
        pg = +1;
    } PGDNcnt *= !!window_pressed(app, TK_PAGEDN);

    // issue browser
    if( window_trigger(app, TK_LEFT)  ) { for(--up; (selected+up) >= 0 && (dbgames[selected+up]&0xFF) <= 1; --up ) ; }
    if( window_trigger(app, TK_RIGHT) ) { for(++up; (selected+up) < numgames && (dbgames[selected+up]&0xFF) <= 1; ++up ) ; }

    for(;up < 0;++up) {
        --selected;
        if( selected < scroll ) --scroll;
    }
    for(;up > 0;--up) {
        ++selected;
        if( selected >= (scroll+ENTRIES) ) ++scroll;
    }
    for(;pg < 0;++pg) {
        if( selected != scroll ) selected = scroll;
        else scroll -= ENTRIES, selected -= ENTRIES;
    }
    for(;pg > 0;--pg) {
        if( selected != scroll+ENTRIES-1 ) selected = scroll+ENTRIES-1;
        else scroll += ENTRIES, selected += ENTRIES;
    }

    scroll = scroll < 0 ? 0 : scroll >= numgames - ENTRIES ? numgames-ENTRIES-1 : scroll;
    selected = selected < scroll ? scroll : selected >= (scroll + ENTRIES + 1) ? scroll + ENTRIES : selected;
    selected = selected < 0 ? 0 : selected >= numgames ? numgames-1 : selected;


        static int chars[16] = {0}, chars_count = -1;
        #define RESET_INPUTBOX() do { memset(chars, 0, sizeof(int)*16); chars_count = -1; } while(0)
        int any = 0;
        // Grab any chars and add them to our buffer.
        for(;;) {
            int c = tigrReadChar(app);
            if (c == 0) break;
            if( window_pressed(app,TK_CONTROL)) break;
            if( c == 8 ) { RESET_INPUTBOX(); break; } // memset(chars, 0, sizeof(int)*16); chars_count = -1; break; }
            if( c == '\t' && chars_count > 0 ) { any = 1; break; }
            if( c <= 32 ) continue;
            else any = 1;
            chars[ chars_count = min(chars_count+1, 15) ] = c;
        }
        // Print out the character buffer too.
        char tmp[1+16*6], *p = tmp;
        for (int n=0;n<16;n++)
            p = tigrEncodeUTF8(p, chars[n]);
        *p = 0;
        char tmp2[16+16*6] = "Find:"; strcat(tmp2, tmp);
        window_printxycol(ui, tmp2, 1,2, tigrRGB(0,192,255));
        if( any ) {
            static char lowercase[1024];
            for(int i = 0; tmp[i]; ++i) tmp[i] |= 32;
            int found = 0;
            if(!found)
            for( int i = scroll+1; i < numgames; ++i ) {
                if (i < 0) continue;
                for(int j = 0; games[i][j]; ++j) lowercase[j+1] = 0, lowercase[j] = games[i][j] | 32;
                if( strstr(lowercase, tmp) ) {
                    scroll = selected = i;
                    found = 1;
                    break;
                }
            }
            if(!found)
            for( int i = 0; i < scroll; ++i ) {
                for(int j = 0; games[i][j]; ++j) lowercase[j+1] = 0, lowercase[j] = games[i][j] | 32;
                if( strstr(lowercase, tmp) ) {
                    scroll = selected = i;
                    found = 1;
                    break;
                }
            }
        }

    if( window_pressed(app, TK_CONTROL) || window_trigger(app, TK_SPACE) ) {
        int update = 0;
        int starred = dbgames[selected] >> 8;
        int color = dbgames[selected] & 0xFF;;
        if( window_trigger(app, TK_SPACE) ) color = (color+1) % 4, update = 1;
        if( window_trigger(app, 'D') ) starred = starred != 'D' ? 'D' : 0, update = 1; // disk error
        if( window_trigger(app, 'T') ) starred = starred != 'T' ? 'T' : 0, update = 1; // tape error
        if( window_trigger(app, 'I') ) starred = starred != 'I' ? 'I' : 0, update = 1; // i/o ports error
        if( window_trigger(app, 'R') ) starred = starred != 'R' ? 'R' : 0, update = 1; // rom/bios error
        if( window_trigger(app, 'E') ) starred = starred != 'E' ? 'E' : 0, update = 1; // emulation error
        if( window_trigger(app, 'Z') ) starred = starred != 'Z' ? 'Z' : 0, update = 1; // zip error
        if( window_trigger(app, 'S') ) starred = starred != 'S' ? 'S' : 0, update = 1; // star
        if( window_trigger(app, '3') ) starred = starred != '3' ? '3' : 0, update = 1; // +3 only error
        if( window_trigger(app, '4') ) starred = starred != '4' ? '4' : 0, update = 1; // 48K only error
        if( window_trigger(app, '1') ) starred = starred != '1' ? '1' : 0, update = 1; // 128K only error
        if( window_trigger(app, '0') ) starred = starred != '0' ? '0' : 0, update = 1; // USR0 only error
        if( window_trigger(app, 'A') ) starred = starred != 'A' ? 'A' : 0, update = 1; // ay/audio error
        if( window_trigger(app, 'V') ) starred = starred != 'V' ? 'V' : 0, update = 1; // video/vram error
        if( window_trigger(app, 'H') ) starred = starred != 'H' ? 'H' : 0, update = 1; // hardware error
        if( window_trigger(app, 'M') ) starred = starred != 'M' ? 'M' : 0, update = 1; // mem/multiload error
        if(update) {
            dbgames[selected] = color + (starred << 8);
            db_set(games[selected], dbgames[selected]);
        }
    }

    if( window_trigger(app, TK_RETURN) ) {
        active = 0;
        RESET_INPUTBOX();

        bool insert_next_disk_or_tape = false;
        if( last_load ) {
            if( 0 != strcmp(games[selected], last_load) ) {
                const char *a1 = games[selected], *a2 = last_load;

                // basenames and their lengths
                const char *b1 = strrchr(a1, '/') ? strrchr(a1, '/')+1 : a1; int l1 = strlen(b1);
                const char *b2 = strrchr(a2, '/') ? strrchr(a2, '/')+1 : a2; int l2 = strlen(b2);
                // printf("%s(%d) %s(%d)\n", b1,l1, b2,l2);

                // multi-load tapes and disks are well named (eg, Mutants - Side 1.tzx). 
                // following oneliner hack prevents some small filenames to be catched in the 
                // diff trap below. eg, 1942.tzx / 1943.tzx; they do not belong to each other
                // albeit their diff is exactly `1`.
                if( l1 > 8 )

                if( l1 == l2 ) {
                    int diff = 0;
                    for( int i = 0; i < l1; ++i ) {
                        diff += b1[i] - b2[i];
                    }
                    insert_next_disk_or_tape = diff == 1;
                }
            }
        }
        bool must_clear = insert_next_disk_or_tape || strstr(games[selected], ".pok") || strstr(games[selected], ".POK") ? 0 : 1;

        if(must_clear)
        clear(window_pressed(app, TK_SHIFT) ? 48 : strstri(games[selected], ".dsk") ? 300 : 128);

        if( load(games[selected], must_clear, window_pressed(app,TK_CONTROL) ? 1:0) ) {
            window_title(app, va("Spectral%s %s%d - %s", FLAGS & DEV ? " DEV" : "", ZX > 128 ? "+":"", ZX > 128 ? ZX/100:ZX, 1+strrchr(games[selected], DIR_SEP)));

            // clear window keys so the current key presses are not being sent to the 
            // next emulation frame. @fixme: use ZXKeyUpdate(); instead
            memset(tigrInternal(app)->keys, 0, sizeof(tigrInternal(app)->keys));
            memset(tigrInternal(app)->prev, 0, sizeof(tigrInternal(app)->prev));
        }

        return 1;
    }

    return 0;
}

unsigned dasm_pc;
char dasm_str[128] = {0};
uint8_t z80dasm_read(void *user_data) {
    uint8_t data = READ8(dasm_pc);
    return ++dasm_pc, data;
}
uint8_t z80dasm_write(char c, void *user_data) {
    char buf[2] = { c/*tolower(c)*/, 0};
    strncat(dasm_str, buf, 128);
    return 0;
}
char* z80dasm(unsigned pc) {
    int bytes = (*dasm_str = 0, z80dasm_op(dasm_pc = pc, z80dasm_read, z80dasm_write, NULL) - pc);

    char hexdump[128], *ptr = hexdump;
    for(int x=0;x<bytes;++x)
    ptr += sprintf(ptr, "%s%02X", " "+(!x), READ8(pc+x));

    char bank[8];
    sprintf(bank, "%d%d%d%d", !!(page128&16), (page128&8?7:5), 2, page128&7);

    return va("%d%s %04X: %-13s  %s", bytes, bank, pc, dasm_str, hexdump);
}

void dis(unsigned pc, unsigned lines, FILE *fp) {
    for( unsigned y = 0; y < lines; ++y ) {
        char *line = z80dasm(pc);
        pc += line[0] - '0';
        fputs(line+1, fp);
        fputs("\n", fp);
    }
}


int main() {
    // install icon hooks for any upcoming window or modal creation
    window_override_icons();

    // convert relative paths
    for( int i = 1; i < __argc; ++i ) {
        if( __argv[i][0] != '-' ) {
            char full[MAX_PATH] = {0};
            realpath(__argv[i], full);
            __argv[i] = strdup(full); // @leak
        }
    }

    // initialize tests
    printer = stdout;
#if FLAGS & TESTS
    {
        if( __argc <= 1 ) die("error: no test file provided");
        printer = fopen(va("%s.txt", __argv[1]), "wt"); //"a+t");
        if(!printer) die("cant open file for logging");

        boost_on = 1;
    }
#endif

    {
        // relocate cwd to exe folder (relative paths wont work from this point)
        char path[MAX_PATH]={0};
        GetModuleFileName(0,path,MAX_PATH);
        *strrchr(path, '\\') = '\0';
        SetCurrentDirectoryA(path);

        // scan files
        {
            float dt;
            dt = tigrTime();
            rescan();
            dt += tigrTime();
            printf("%5.2fms rescan\n", dt*1000);
        }
    }

    // app
    app = window_open(_32+256+_32, _24+192+_24, va("Spectral%s", FLAGS & DEV ? " DEV" : "") );
    ui = window_bitmap(_320, _240);

    // postfx
    crt(ZX_CRT = !!(FLAGS & CRT));

    // must be as close to frame() as possible
    audio_init();

    // zx
    clear(ZX);

    // import state
    for( FILE *state = fopen("spectral.sav","rb"); state; fclose(state), state = 0) {
        import_state(state);
        pins = z80_prefetch(&cpu, cpu.pc);
    }

    // main loop
    do {

        // tape_hooks();

        int accelerated = ZX_FAST ? boost_on || (mic_on && mic_has_tape) : 0;
        if( accelerated && active ) accelerated = 0;

        // z80, ula, audio, etc
        // static int frame = 0; ++frame;
        int do_drawmode = 1; // no render (<0), single frame (0), scanlines (1)
        int do_flashbit = accelerated ? 0 : 1;
        int do_runahead = accelerated || ZX >= 300 ? 0 : ZX_RUNAHEAD;

#if !(FLAGS & RUNAHEAD)
        do_runahead = 0;
#endif

#if FLAGS & TESTS
        static byte even = 0; ++even;
        do_drawmode = even & 1; // be fast. 50% frames not drawn. the other 50% are drawn in the fastest mode
#endif
#if FLAGS & TESTS
        // monitor test for completion
        static byte check_tests = 0;
        if( !check_tests++ )
        {
            static unsigned prev = 0;
            static unsigned stalled = 0;

            struct stat st;
            if( fstat(fileno(printer), &st) == 0 ) {
                if( prev == st.st_size ) ++stalled;
                else prev = st.st_size, stalled = 0;
            }

            // finish test after being idle for 15,000,000 frames
            if( stalled >= (50*300000/256) ) {
                fprintf(printer, "Quitting test because of inactivity.\n");
                exit(0);
            }
        }
#endif

        input();

        static byte counter = 0; // flip flash every 16 frames @ 50hz
        if( !((++counter) & 15) ) if(do_flashbit) ZXFlashFlag ^= 1;

if( do_runahead == 0 ) {
        do_audio = 1;
        frame(do_drawmode); //accelerated ? (frame%50?0:1) : 1 );
} else {
        do_audio = 0;
        frame(-1);

        checkpoint_save(10);

        do_audio = 1;
        frame(do_drawmode);

        checkpoint_load(10);
}

        // game browser
        int game_loaded = game_browser();

        if( do_runahead == 1 && !game_loaded /*&& !accelerated*/ ) {
//            checkpoint_load(10);
        }

        // measure time & frame lock (50.01 fps)
        float dt;
        if(accelerated) {
            dt = tigrTime();
            // constant time flashing when loading accelerated tapes (every 16 frames @ 50hz)
            static float accum = 0; accum += dt;
            if( accum >= 0.32f ) accum = 0, ZXFlashFlag ^= 1;
        } else {
#if 0 // no lock
            dt = tigrTime();
#elif 0 // naive
            sys_sleep(1000/50.f); // 50 -> 39 fps
            dt = tigrTime();
#elif 0 // less naive
            dt = tigrTime();
            if( dt < (1000/50.f) ) sys_sleep( (1000/50) - dt );
#else // accurate (beware of CPU usage)
            // be nice to os
            sys_sleep(5);
            // complete with shortest sleeps (yields) until we hit target fps
            dt = tigrTime();
            for( float target_fps = 1.f/(ZX < 128 ? 50.08:50.01); dt < target_fps; ) {
                sys_yield();
                dt += tigrTime();
            }
#endif
        }

        // calc fps
        static float fps = 0;
        static int frames = 0; ++frames;
        static double time_now = 0; time_now += dt;
        if( time_now >= 1 ) { fps = frames / time_now; time_now = frames = 0; }

        // app timer
        static double accum = 0;
        if(mic_on && mic_has_tape) accum += dt;

        // ui
        {
            // compatibility stats
            int total = numok+numwarn+numerr;
            if(total && active) {
            TPixel white = {255,255,255,255}, black = {0,0,0,255}, *bar = &ui->pix[0 + _239 * _320];
            int num1 = (numok * (float)_319) / total;
            int num2 = (numwarn * (float)_319) / total;
            int num3 = (numerr * (float)_319) / total; if((num1+num2+num3)<_319) num1 += _319 - (num1+num2+num3);
            for( int x = 0; x <= num1; ++x ) bar[x-320]=bar[x] = tigrRGB(64,255,64);
            for( int x = 0; x <= num2; ++x ) bar[x+num1-320]=bar[x+num1] = tigrRGB(255,192,64);
            for( int x = 0; x <= num3; ++x ) bar[x+num1+num2-320]=bar[x+num1+num2] = tigrRGB(255,64,64);
            static char compat[64];
            snprintf(compat, 64, "  OK:%04.1f%%     ENTER:128, +SHIFT:48, +CTRL:Try turbo", (total-numerr) * 100.f / (total+!total));
            window_printxy(ui, compat, 0,(_240-12.0)/11);
            }

            // stats & debug
            static char info[64] = "";
            char *ptr = info;
            ptr += sprintf(ptr, "%dm%02ds ", (unsigned)(accum) / 60, (unsigned)(accum) % 60);
            ptr += sprintf(ptr, "%5.2ffps%s %d mem%d%d%d%d%s ", fps, do_runahead ? "!":"", ZX, !!(page128&16), (page128&8?7:5), 2, page128&7, page128&32?"!":"");
            ptr += sprintf(ptr, "%02x %02x %04x ", page128, page2a, PC(cpu));

            ptr += sprintf(ptr, "x%.4f ", azimuth);
//          ptr += sprintf(ptr, "%03d %05x", autotape_counter, autotape_indicators);
#if 0
            ptr += 1/*mic_on*/ ? sprintf(ptr, "%03d/%s(%c) ", mic_last_block, mic_last_type, mic_invert_polarity?'-':'+') : 0;
#else
            ptr += mic_on ? sprintf(ptr, "%c%c ", mic_invert_polarity?'-':'+', voc ? voc[voc_pos/4] & 0x7f & ~32 : 'S') : 0;
#endif

#if FLAGS & DEV
            if(1)
#else
            if( window_pressed(app, TK_SHIFT) || active )
#endif
	            window_print(ui, info);

            // tape progress
            float pct = (voc_pos/4) / (float)(voclen+!voclen);
            if( mic_on /* && (pct < 1.00f)*/ ) {
                TPixel white = {255,255,255,255}, black = {0,0,0,255}, *bar = &ui->pix[0 + (ZX_CRT ? 2 : 0) * _320];

                // bars & progress
                unsigned mark = pct * _320;
                for( int x = 0; x < _320; ++x ) bar[x] = bar[x+2*_320] = white;
                for( int x = 0; x<=mark; ++x ) bar[x+_320] = white; bar[_320-1+_320] = white;
                for( int x = 0; x < _320; ++x ) if(mic_preview[x]) bar[x+1*_320] = white;
                // triangle marker (top)
                bar[mark+4*_320] = white;
                for(int i = -1; i <= +1; ++i) if((mark+i)>=0 && (mark+i)<_320) bar[mark+i+5*_320] = white;
                for(int i = -2; i <= +2; ++i) if((mark+i)>=0 && (mark+i)<_320) bar[mark+i+6*_320] = white;
                // mouse seeking
                int mx, my, mbuttons;
                tigrMouse(app, &mx, &my, &mbuttons);
                if( mbuttons && my > 0 && my <= 15 ) {
                    mx = mx < 0 ? 0 : mx > _320 ? _320 : mx;
                    float target = (mx / (float)_320) * (float)(voclen+!voclen);
                    voc_pos = (voc_pos/4) * 0.95f + target * 0.05f ; // animate seeking
                    voc_pos *= 4;
                }
            }

            #define CLAMP(v, minv, maxv) ((v) < (minv) ? (minv) : (v) > (maxv) ? (maxv) : (v))
            #define REMAP(var, src_min, src_max, dst_min, dst_max) \
                (dst_min + ((CLAMP(var, src_min, src_max) - src_min) / (float)(src_max - src_min)) * (dst_max - dst_min))

#if FLAGS & DEV
            // azimuth slider
            if( !active ) {
                TPixel white = {255,255,255,255}, black = {0,0,0,255}, *bar = &ui->pix[0 + (_240-7) * _320];
                unsigned mark = REMAP(azimuth, 1.0,1.2, 0,1) * _320;
                // triangle marker (bottom)
                for(int i = -2; i <= +2; ++i) if((mark+i)>=0 && (mark+i)<_320) bar[mark+i+0*_320] = white;
                for(int i = -1; i <= +1; ++i) if((mark+i)>=0 && (mark+i)<_320) bar[mark+i+1*_320] = white;
                bar[mark+2*_320] = white;
                bar += _320 * 4;
                // bars & progress
                for( int x = 0; x < _320; ++x ) bar[x] = bar[x+2*_320] = white;
                for( int x = 0; x<=mark; ++x ) bar[x+_320] = white; bar[_320-1+_320] = white;
                // mouse seeking
                int mx, my, mbuttons;
                tigrMouse(app, &mx, &my, &mbuttons);
                if( (mbuttons/*&4*/) && my >= (_240-10) && my < _240 ) {
                    float target = REMAP(mx, 0,_320, 0.98,1.2);
                    azimuth = azimuth * 0.95f + target * 0.05f ; // animate seeking
                }
            }
#endif
        }

        // draw ui on top
        tigrBlitAlpha(app, ui, 0,0, 0,0, _320,_240, 1.0f);

        // flush
        window_update(app);

        // parse drag 'n drops. reload if needed
        for( char **list = tigrDropFiles(app); list; list = 0)
        for( int i = 0; list[i]; ++i ) {
            int do_clear = 1;
            int do_rompatch = !!window_pressed(app,TK_CONTROL);

            // if( do_clear ) clear(window_pressed(app,TK_SHIFT) ? 48 : 128);

            if( !load(list[i], do_clear, do_rompatch) ) {
                if( !load_shader( list[i] ) ) {
                    warning(va("cannot open '%s' file\n", __argv[i]));
                }
            }
        }

        // parse cmdline. reload if needed
        do_once
        for( int i = 1; i < __argc; ++i )
        if( __argv[i][0] != '-' ) {
            int do_clear = 1;
            int do_rompatch = !!window_pressed(app,TK_CONTROL);

            // if( do_clear ) clear(window_pressed(app,TK_SHIFT) ? 48 : 128);

#if FLAGS & TESTS
            config(48);
            do_rompatch = 1;
#endif

            if( !load(__argv[i], do_clear, do_rompatch) ) {
                if( !load_shader( __argv[i] ) ) {
                    warning(va("cannot open '%s' file\n", __argv[i]));
                }
            }
        } else {
            int total = numok+numwarn+numerr;

            if( !(FLAGS & DEV) )
            warning(va(SPECTRAL "\n%d games (%d%%)\n\n%s", numgames, 100 - (numerr * 100 / (total + !total)), static_options()));
        }

    } while( window_alive(app) );

    // export state
    while( !z80_opdone(&cpu) ) pins = z80_tick(&cpu, pins);
    for( FILE *state = fopen("spectral.sav","wb"); state; fclose(state), state = 0) {
        export_state(state);
    }

    window_close(app);
    return 0;
}