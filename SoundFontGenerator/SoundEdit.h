
/*
 * SoundEdit.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#ifndef JACKAPI
#include <portaudio.h>
#endif
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <set>
#include <vector>
#include <string>
#include <sndfile.hh>
#include <fstream>
#include <limits>
#include <cstdint>

#include "SupportedFormats.h"
#include "xwidgets.h"
#include "xfile-dialog.h"
#include "AudioFile.h"


#pragma once

#ifndef AUDIOLOOPERUI_H
#define AUDIOLOOPERUI_H

/****************************************************************
    class SoundEditUi - create the GUI for alooper
****************************************************************/

class SoundEditUi
{
public:
    Widget_t *w;
    ParallelThread pa;
    AudioFile af;
    
    uint32_t jack_sr;
    uint32_t position;
    uint32_t loopPoint_l;
    uint32_t loopPoint_r;
    uint32_t frameSize;

    float gain;

    std::string filename;

    bool loadNew;
    bool play;
    bool stop;
    bool ready;

    SoundEditUi() : af(), pre_af() {
        jack_sr = 0;
        position = 0;
        loopPoint_l = 0;
        loopPoint_r = 1000;
        frameSize = 0;
        gain = std::pow(1e+01, 0.05 * 0.0);
        is_loaded = false;
        loadNew = false;
        play = false;
        stop = false;
        ready = true;
        stream = nullptr;
    };

    ~SoundEditUi() {
        pa.stop();
    };

/****************************************************************
                      public function calls
****************************************************************/

    // stop background threads and quit main window
    void onExit() {
        pa.stop();
        quit(w);
    }
    
    // receive Sample Rate from audio back-end
    void setJackSampleRate(uint32_t sr) {
        jack_sr = sr;        
    }

    // receive stream object from portaudio to check 
    // if the server is actual running
    void setPaStream(PaStream* stream_) {
        stream = stream_;
    }

    // receive a file name from the File Browser or the command-line
    static void dialog_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        if (!Pa_IsStreamActive(self->stream)) return;
        if(user_data !=NULL) {
            self->filename = *(char**)user_data;
            self->loadFile();
        } else {
            std::cerr << "no file selected" <<std::endl;
        }
    }

    // load a audio file in background process
    void loadFile() {
        read_soundfile(filename.c_str());
    }

/****************************************************************
                      main window
****************************************************************/

    // create the main GUI
    void createGUI(Xputty *app) {
        w_top = create_window(app, os_get_root_window(app, IS_WINDOW), 0, 0, 440, 190);
        widget_set_title(w_top, "sf2generator");
        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        widget_set_dnd_aware(w_top);
        #endif
        w_top->parent_struct = (void*)this;
        w_top->func.dnd_notify_callback = dnd_load_response;
        w_top->func.key_press_callback = key_press;
        w_top->func.resize_notify_callback = resize_callback;
        os_set_window_min_size(w_top, 335, 85, 440, 190);

        w = create_widget(app, w_top, 0, 0, 440, 190);

        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        widget_set_dnd_aware(w);
        #endif
        w->parent_struct = (void*)this;
        w->parent = w_top;
        w->scale.gravity = NORTHWEST;
        w->func.expose_callback = draw_window;
        w->func.dnd_notify_callback = dnd_load_response;
        w->func.key_press_callback = key_press;

        loopMark_L = add_hslider(w, "",15, 2, 18, 18);
        loopMark_L->scale.gravity = NONE;
        loopMark_L->parent_struct = (void*)this;
        loopMark_L->adj_x = add_adjustment(loopMark_L,0.0, 0.0, 0.0, 1000.0,1.0, CL_METER);
        loopMark_L->adj = loopMark_L->adj_x;
        add_tooltip(loopMark_L, "Set left loop point ");
        loopMark_L->func.expose_callback = draw_slider;
        loopMark_L->func.button_release_callback = slider_l_released;
        loopMark_L->func.motion_callback = move_loopMark_L;
        loopMark_L->func.value_changed_callback = slider_l_changed_callback;

        loopMark_R = add_hslider(w, "",415, 2, 18, 18);
        loopMark_R->scale.gravity = NONE;
        loopMark_R->parent_struct = (void*)this;
        loopMark_R->adj_x = add_adjustment(loopMark_R,0.0, 0.0, -1000.0, 0.0,1.0, CL_METER);
        loopMark_R->adj = loopMark_R->adj_x;
        add_tooltip(loopMark_R, "Set right loop point ");
        loopMark_R->func.expose_callback = draw_slider;
        loopMark_R->func.button_release_callback = slider_r_released;
        loopMark_R->func.motion_callback = move_loopMark_R;
        loopMark_R->func.value_changed_callback = slider_r_changed_callback;

        wview = add_waveview(w, "", 20, 20, 400, 120);
        wview->scale.gravity = NORTHWEST;
        wview->parent_struct = (void*)this;
        wview->adj_x = add_adjustment(wview,0.0, 0.0, 0.0, 1000.0,1.0, CL_METER);
        wview->adj = wview->adj_x;
        wview->func.expose_callback = draw_wview;
        wview->func.button_release_callback = set_playhead;
        wview->func.key_press_callback = key_press;

        filebutton = add_file_button(w, 20, 150, 30, 30, getenv("HOME") ? getenv("HOME") : PATH_SEPARATOR, "audio");
        filebutton->scale.gravity = SOUTHEAST;
        filebutton->parent_struct = (void*)this;
        widget_get_png(filebutton, LDVAR(load__png));
        filebutton->flags |= HAS_TOOLTIP;
        add_tooltip(filebutton, "Load audio file");
        filebutton->func.user_callback = dialog_response;

        saveLoop = add_xsave_file_button(w, 60, 150, 30, 30, getenv("HOME") ? getenv("HOME") : PATH_SEPARATOR, ".sf2");
        saveLoop->parent_struct = (void*)this;
        saveLoop->scale.gravity = SOUTHEAST;
        saveLoop->flags |= HAS_TOOLTIP;
        add_tooltip(saveLoop, "Save as Sound Font (sf2)");
        saveLoop->func.user_callback = write_soundfile;

        volume = add_knob(w, "dB",265,150,28,28);
        volume->parent_struct = (void*)this;
        volume->scale.gravity = SOUTHWEST;
        volume->flags |= HAS_TOOLTIP;
        add_tooltip(volume, "Volume (dB)");
        set_adjustment(volume->adj, 0.0, 0.0, -20.0, 6.0, 0.1, CL_CONTINUOS);
        volume->func.expose_callback = draw_knob;
        volume->func.value_changed_callback = volume_callback;

        playbutton = add_image_toggle_button(w, "", 360, 150, 30, 30);
        playbutton->scale.gravity = SOUTHWEST;
        playbutton->parent_struct = (void*)this;
        widget_get_png(playbutton, LDVAR(play_png));
        playbutton->flags |= HAS_TOOLTIP;
        add_tooltip(playbutton, "Play");
        playbutton->func.value_changed_callback = button_playbutton_callback;

        w_quit = add_button(w, "", 390, 150, 30, 30);
        w_quit->parent_struct = (void*)this;
        widget_get_png(w_quit, LDVAR(exit__png));
        w_quit->scale.gravity = SOUTHWEST;
        w_quit->flags |= HAS_TOOLTIP;
        add_tooltip(w_quit, "Exit");
         w_quit->func.value_changed_callback = button_quit_callback;

        widget_show_all(w_top);

        pa.startTimeout(60);
        pa.set<SoundEditUi, &SoundEditUi::updateUI>(this);

    }

private:
    Widget_t *w_top;
    Widget_t *w_quit;
    Widget_t *filebutton;
    Widget_t *wview;
    Widget_t *loopMark_L;
    Widget_t *loopMark_R;
    Widget_t *playbutton;
    Widget_t *volume;
    Widget_t *lview;
    Widget_t *saveLoop;

    SupportedFormats supportedFormats;
    AudioFile pre_af;

    PaStream* stream;

    std::mutex WMutex;

    bool is_loaded;
    std::string newLabel;

/****************************************************************
                    Sound File loading
****************************************************************/

    // when Sound File loading fail, clear wave view and reset tittle
    void failToLoad() {
        loadNew = true;
        update_waveview(wview, af.samples, af.samplesize);
        widget_set_title(w_top, "sf2generator");
    }

    // load a Sound File when pre-load is the wrong file
    void load_soundfile(const char* file) {
        af.channels = 0;
        af.samplesize = 0;
        af.samplerate = 0;
        position = 0;

        ready = false;
        is_loaded = af.getAudioFile(file, jack_sr);
        if (!is_loaded) failToLoad();
    }

    // load Sound File data into memory
    void read_soundfile(const char* file, bool haveLoopPoints = false) {
        load_soundfile(file);
        is_loaded = false;
        loadNew = true;
        if (af.samples) {
            adj_set_max_value(wview->adj, (float)af.samplesize);
            //adj_set_max_value(loopMark_L->adj, (float)af.samplesize*0.5);
            adj_set_state(loopMark_L->adj, 0.0);
            loopPoint_l = 0;
            //adj_set_max_value(loopMark_R->adj, (float)af.samplesize*0.5);
            adj_set_state(loopMark_R->adj,1.0);
            loopPoint_r = af.samplesize;
            
            update_waveview(wview, af.samples, af.samplesize);
            char name[256];
            strncpy(name, file, 255);
           // widget_set_title(w_top, basename(name));
        } else {
            af.samplesize = 0;
            std::cerr << "Error: could not resample file" << std::endl;
            failToLoad();
        }
        ready = true;
    }

/****************************************************************
                    save Sound File
****************************************************************/

    // save a loop to file
    static void write_soundfile(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        if(user_data !=NULL && strlen(*(const char**)user_data)) {
            SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
            if (!self->af.samples) return;
            std::string lname(*(const char**)user_data);
            self->af.savesf2(lname, self->loopPoint_l, self->loopPoint_r, self->jack_sr, self->gain);
        }
    }

/****************************************************************
            drag and drop handling for the main window
****************************************************************/

    static void dnd_load_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        if (!Pa_IsStreamActive(self->stream)) return;
        if (user_data != NULL) {
            char* dndfile = NULL;
            dndfile = strtok(*(char**)user_data, "\r\n");
            while (dndfile != NULL) {
                if (self->supportedFormats.isSupported(dndfile) ) {
                    self->filename = dndfile;
                    self->loadFile();
                    break;
                } else {
                    std::cerr << "Unrecognized file extension: " << dndfile << std::endl;
                }
                dndfile = strtok(NULL, "\r\n");
            }
        }
    }

/****************************************************************
            Play head (called from timeout thread) 
****************************************************************/

    static void dummy_callback(void *w_, void* user_data) {}

    // frequently (60ms) update the wave view widget for playhead position
    // triggered from the timeout background thread 
    void updateUI() {
        static int waitOne = 0;
        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        XLockDisplay(w->app->dpy);
        #endif
        wview->func.adj_callback = dummy_callback;
        if (ready) adj_set_value(wview->adj, (float) position);
        else {
            waitOne++;
            if (waitOne > 2) {
                transparent_draw(wview, nullptr);
                waitOne = 0;
            }
        }
        expose_widget(wview);
        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        XFlush(w->app->dpy);
        XUnlockDisplay(w->app->dpy);
        #endif
        wview->func.adj_callback = transparent_draw;
    }

/****************************************************************
                      Button callbacks 
****************************************************************/

    // quit
    static void button_quit_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        if (w->flags & HAS_POINTER && !*(int*)user_data){
            self->onExit();
        }
    }

    static void fxdialog_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        FileButton *filebutton = (FileButton *)w->private_struct;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        self->play = false;
        if(user_data !=NULL) {
            char *tmp = strdup(*(const char**)user_data);
            free(filebutton->last_path);
            filebutton->last_path = NULL;
            filebutton->last_path = strdup(dirname(tmp));
            filebutton->path = filebutton->last_path;
            free(tmp);
        }
        w->func.user_callback(w,user_data);
        filebutton->is_active = false;
        adj_set_value(w->adj,0.0);
        if (adj_get_value(self->playbutton->adj))
             self->play = true;
    }

    static void fxbutton_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        FileButton *filebutton = (FileButton *)w->private_struct;
        if (w->flags & HAS_POINTER && adj_get_value(w->adj)){
            filebutton->w = save_file_dialog(w,filebutton->path,filebutton->filter);
    #ifdef _OS_UNIX_
            Atom wmStateAbove = XInternAtom(w->app->dpy, "_NET_WM_STATE_ABOVE", 1 );
            Atom wmNetWmState = XInternAtom(w->app->dpy, "_NET_WM_STATE", 1 );
            XChangeProperty(w->app->dpy, filebutton->w->widget, wmNetWmState, XA_ATOM, 32, 
                PropModeReplace, (unsigned char *) &wmStateAbove, 1); 
    #elif defined _WIN32
            os_set_transient_for_hint(w, filebutton->w);
    #endif
            filebutton->is_active = true;
        } else if (w->flags & HAS_POINTER && !adj_get_value(w->adj)){
            if(filebutton->is_active)
                destroy_widget(filebutton->w,w->app);
        }
    }

    #if defined(_WIN32)
    void getWindowDecorationSize(int *width, int *height) {
        DWORD dwStyle = WS_OVERLAPPEDWINDOW;
        DWORD dwExStyle = WS_EX_CONTROLPARENT | WS_EX_ACCEPTFILES;
        RECT Rect = {0};
        BOOL bMenu = false;
        Rect.right = *width;
        Rect.bottom = *height;
        if (AdjustWindowRectEx(&Rect, dwStyle, bMenu, dwExStyle)) {
            *width = Rect.right - Rect.left;
            *height = Rect.bottom - Rect.top;
        }
       
    }
    #else
    void getWindowDecorationSize(int *width, int *height) {
        Atom type;
        int format;
        unsigned long  count = 0, remaining;
        unsigned char* data = 0;
        long* extents;
        Atom _NET_FRAME_EXTENTS = XInternAtom(w_top->app->dpy, "_NET_FRAME_EXTENTS", True);
        XGetWindowProperty(w_top->app->dpy, w_top->widget, _NET_FRAME_EXTENTS,
            0, 4, False, AnyPropertyType,&type, &format, &count, &remaining, &data);
        extents = (long*) data;
        *height = - static_cast<int>(extents[2])/2;
        *width = - static_cast<int>(extents[0])/2;
    }
    #endif

    // toggle playbutton button with space bar
    static void key_press(void *w_, void *key_, void *user_data) {
        Widget_t *w = (Widget_t*)w_;
        if (!w) return;
        XKeyEvent *key = (XKeyEvent*)key_;
        if (!key) return;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        if (key->keycode == XKeysymToKeycode(w->app->dpy, XK_space)) {
            adj_set_value(self->playbutton->adj, !adj_get_value(self->playbutton->adj));
            self->play = adj_get_value(self->playbutton->adj) ? true : false;
        } else if (key->keycode == XKeysymToKeycode(w->app->dpy, XK_q)) {
            self->onExit();
        } else if ((key->state & ControlMask) && (key->keycode == XKeysymToKeycode(w->app->dpy, XK_plus))) {
            int x = 1;
            int y = 1;
            #if defined(_WIN32)
            self->getWindowDecorationSize(&x, &y);
            #endif
            os_resize_window(self->w->app->dpy, self->w, self->w->width + x, self->w->height + y);
            expose_widget(self->w);
        } else if ((key->state & ControlMask) && (key->keycode == XKeysymToKeycode(w->app->dpy, XK_minus))) {
            int x = 1;
            int y = 1;
            #if defined(_WIN32)
            self->getWindowDecorationSize(&x, &y);
            #endif
            os_resize_window(self->w->app->dpy, self->w, self->w->width + x -2, self->w->height + y -2);
            expose_widget(self->w);
        }
    }

    // playbutton
    static void button_playbutton_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        if (adj_get_value(w->adj)){
            self->play = true;
        } else self->play = false;
    }

    // set left loop point by value change
    static void slider_l_changed_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        float st = adj_get_state(w->adj);
        uint32_t lp = (self->af.samplesize) * st;
        if (lp > self->position) {
            lp = self->position;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        adj_set_state(w->adj, st);
        int width = self->w_top->width-40;
        os_move_window(self->w->app->dpy, w, 15+ (width * st), 2);
        self->loopPoint_l = lp;
    }

    // set left loop point by mouse wheel
    static void slider_l_released(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        if (w->flags & HAS_POINTER) {
            if(xbutton->button == Button4) {
                adj_set_value(w->adj, adj_get_value(w->adj) + 1.0);
            } else if(xbutton->button == Button5) {
                adj_set_value(w->adj, adj_get_value(w->adj) - 1.0);
            }
        }
        expose_widget(w);
    }

    // move left loop point following the mouse pointer
    static void move_loopMark_L(void *w_, void *xmotion_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XMotionEvent *xmotion = (XMotionEvent*)xmotion_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        Widget_t *p = (Widget_t*)w->parent;
        int x1, y1;
        os_translate_coords(w, w->widget, p->widget, xmotion->x, 0, &x1, &y1);
        int width = self->w_top->width-40;
        int pos = max(15, min (width+15,x1-5));
        float st =  (float)( (float)(pos-15.0)/(float)width);
        uint32_t lp = (self->af.samplesize) * st;
        if (lp > self->position) {
            lp = self->position;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        adj_set_state(w->adj, st);
    }

    // set right loop point by value changes
    static void slider_r_changed_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        float st = adj_get_state(w->adj);
        uint32_t lp = (self->af.samplesize * st);
        if (lp < self->position) {
            lp = self->position;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        adj_set_state(w->adj, st);
        int width = self->w_top->width-40;
        os_move_window(self->w->app->dpy, w, 15 + (width * st), 2);
        self->loopPoint_r = lp;
    }

    // set right loop point by mouse wheel
    static void slider_r_released(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        if (w->flags & HAS_POINTER) {
            if(xbutton->button == Button4) {
                adj_set_value(w->adj, adj_get_value(w->adj) - 1.0);
            } else if(xbutton->button == Button5) {
                adj_set_value(w->adj, adj_get_value(w->adj) + 1.0);
            }
        }
        expose_widget(w);
    }

    // move right loop point following the mouse pointer
    static void move_loopMark_R(void *w_, void *xmotion_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XMotionEvent *xmotion = (XMotionEvent*)xmotion_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        Widget_t *p = (Widget_t*)w->parent;
        int x1, y1;
        os_translate_coords(w, w->widget, p->widget, xmotion->x, 0, &x1, &y1);
        int width = self->w_top->width-40;
        int pos = max(15, min (width+15,x1-5));
        float st =  (float)( (float)(pos-15.0)/(float)width);
         uint32_t lp = (self->af.samplesize * st);
        if (lp < self->position) {
            lp = self->position;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        adj_set_state(w->adj, st);
    }

    // set loop mark positions on window resize
    static void resize_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        float st = adj_get_state(self->loopMark_L->adj);
        int width = w->width-40;
        os_move_window(w->app->dpy, self->loopMark_L, 15+ (width * st), 2);
        st = adj_get_state(self->loopMark_R->adj);
        os_move_window(w->app->dpy, self->loopMark_R, 15+ (width * st), 2);
    }

    // set playhead position to mouse pointer
    static void set_playhead(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        if (w->flags & HAS_POINTER) {
            if(xbutton->state & Button1Mask) {
                Metrics_t metrics;
                os_get_window_metrics(w, &metrics);
                int width = metrics.width;
                int x = xbutton->x;
                float st = max(0.0, min(1.0, static_cast<float>((float)x/(float)width)));
                uint32_t lp = adj_get_max_value(w->adj) * st;
                if (lp > self->loopPoint_r) lp = self->loopPoint_r;
                if (lp < self->loopPoint_l) lp = self->loopPoint_l;
                self->position = lp;
            }
        }
    }

    // volume control
    static void volume_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        self->gain = std::pow(1e+01, 0.05 * adj_get_value(w->adj));
    }

/****************************************************************
                      drawings 
****************************************************************/

    static void draw_slider(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int height = metrics.height;
        if (!metrics.visible) return;
        float center = (float)height/2;
        float upcenter = (float)height;

        use_fg_color_scheme(w, get_color_state(w));
        float point = 5.0;
        cairo_move_to (w->crb, point - 5.0, center);
        cairo_line_to(w->crb, point + 5.0, center);
        cairo_line_to(w->crb, point , upcenter);
        cairo_line_to(w->crb, point - 5.0 , center);
        cairo_fill(w->crb);
    }

    void roundrec(cairo_t *cr, float x, float y, float width, float height, float r) {
        cairo_arc(cr, x+r, y+r, r, M_PI, 3*M_PI/2);
        cairo_arc(cr, x+width-r, y+r, r, 3*M_PI/2, 0);
        cairo_arc(cr, x+width-r, y+height-r, r, 0, M_PI/2);
        cairo_arc(cr, x+r, y+height-r, r, M_PI/2, M_PI);
        cairo_close_path(cr);
    }

    static void draw_knob(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width = metrics.width;
        int height = metrics.height;
        if (!metrics.visible) return;

        const double scale_zero = 20 * (M_PI/180); // defines "dead zone" for knobs
        int arc_offset = 0;
        int knob_x = 0;
        int knob_y = 0;

        int grow = (width > height) ? height:width;
        knob_x = grow-1;
        knob_y = grow-1;
        /** get values for the knob **/

        const int knobx1 = width* 0.5;

        const int knoby1 = height * 0.5;

        const double knobstate = adj_get_state(w->adj_y);
        const double angle = scale_zero + knobstate * 2 * (M_PI - scale_zero);

        const double pointer_off =knob_x/6;
        const double radius = min(knob_x-pointer_off, knob_y-pointer_off) / 2;

        const double add_angle = 90 * (M_PI / 180.);
        // base
        use_base_color_scheme(w, INSENSITIVE_);
        cairo_set_line_width(w->crb,  5.0/w->scale.ascale);
        cairo_arc (w->crb, knobx1+arc_offset, knoby1+arc_offset, radius,
              add_angle + scale_zero, add_angle + scale_zero + 320 * (M_PI/180));
        cairo_stroke(w->crb);

        // indicator
        cairo_set_line_width(w->crb,  3.0/w->scale.ascale);
        cairo_new_sub_path(w->crb);
        cairo_set_source_rgba(w->crb, 0.75, 0.75, 0.75, 1);
        cairo_arc (w->crb,knobx1+arc_offset, knoby1+arc_offset, radius,
              add_angle + scale_zero, add_angle + angle);
        cairo_stroke(w->crb);

        use_text_color_scheme(w, get_color_state(w));
        cairo_text_extents_t extents;
        /** show value on the kob**/
        char s[64];
        float value = adj_get_value(w->adj);
        if (fabs(w->adj->step)>0.09)
            snprintf(s, 63, "%.1f", value);
        else
            snprintf(s, 63, "%.2f", value);
        cairo_set_font_size (w->crb, (w->app->small_font-2)/w->scale.ascale);
        cairo_text_extents(w->crb, s, &extents);
        cairo_move_to (w->crb, knobx1-extents.width/2, knoby1+extents.height/2);
        cairo_show_text(w->crb, s);
        cairo_new_path (w->crb);
    }

    void create_waveview_image(Widget_t *w, int width, int height) {
        cairo_surface_destroy(w->image);
        w->image = NULL;
        w->image = cairo_surface_create_similar (w->surface,
                            CAIRO_CONTENT_COLOR_ALPHA, width, height);
        cairo_t *cri = cairo_create (w->image);

        WaveView_t *wave_view = (WaveView_t*)w->private_struct;
        int half_height_t = height/2;

        cairo_set_line_width(cri,2);
        cairo_set_source_rgba(cri, 0.05, 0.05, 0.05, 1);
        roundrec(cri, 0, 0, width, height, 5);
        cairo_fill_preserve(cri);
        cairo_set_source_rgba(cri, 0.33, 0.33, 0.33, 1);
        cairo_stroke(cri);
        cairo_move_to(cri,2,half_height_t);
        cairo_line_to(cri, width, half_height_t);
        cairo_stroke(cri);

        if (wave_view->size<1 || !ready) return;
        int step = (wave_view->size/width)/af.channels;
        float lstep = (float)(half_height_t)/af.channels;
        cairo_set_line_width(cri,2);
        cairo_set_source_rgba(cri, 0.55, 0.65, 0.55, 1);

        int pos = half_height_t/af.channels;
        for (int c = 0; c < (int)af.channels; c++) {
            cairo_pattern_t *pat = cairo_pattern_create_linear (0, pos, 0, height);
            cairo_pattern_add_color_stop_rgba
                (pat, 0,1.53,0.33,0.33, 1.0);
            cairo_pattern_add_color_stop_rgba
                (pat, 0.7,0.53,0.33,0.33, 1.0);
            cairo_pattern_add_color_stop_rgba
                (pat, 0.3,0.33,0.53,0.33, 1.0);
            cairo_pattern_add_color_stop_rgba
                (pat, 0, 0.55, 0.55, 0.55, 1.0);
            cairo_pattern_set_extend(pat, CAIRO_EXTEND_REFLECT);
            cairo_set_source(cri, pat);
            for (int i=0;i<width-4;i++) {
                cairo_move_to(cri,i+2,pos);
                float w = wave_view->wave[int(c+(i*af.channels)*step)];
                cairo_line_to(cri, i+2,(float)(pos)+ (-w * lstep));
                cairo_line_to(cri, i+2,(float)(pos)+ (w * lstep));
            }
            pos += half_height_t;
            cairo_pattern_destroy (pat);
            pat = nullptr;
        }
        cairo_stroke(cri);
        cairo_destroy(cri);
    }

    static void draw_wview(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width_t = metrics.width;
        int height_t = metrics.height;
        if (!metrics.visible) return;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        int width, height;
        static bool clearImage = false;
        static bool clearImageDone = false;
        if (!self->ready && !clearImageDone) clearImage = true;
        if (w->image) {
            os_get_surface_size(w->image, &width, &height);
            if (((width != width_t || height != height_t) || self->loadNew) && self->ready) {
                self->loadNew = false;
                clearImageDone = false;
                self->create_waveview_image(w, width_t, height_t);
                os_get_surface_size(w->image, &width, &height);
            }
        } else {
            self->create_waveview_image(w, width_t, height_t);
            os_get_surface_size(w->image, &width, &height);
        }
        if (clearImage) {
            clearImage = false;
            clearImageDone = true;
            self->create_waveview_image(w, width_t, height_t);
            os_get_surface_size(w->image, &width, &height);
        }
        cairo_set_source_surface (w->crb, w->image, 0, 0);
        cairo_rectangle(w->crb,0, 0, width, height);
        cairo_fill(w->crb);

        double state = adj_get_state(w->adj);
        cairo_set_source_rgba(w->crb, 0.55, 0.05, 0.05, 1);
        cairo_rectangle(w->crb, (width * state) - 1.5,2,3, height-4);
        cairo_fill(w->crb);

        //int halfWidth = width*0.5;

        double state_l = adj_get_state(self->loopMark_L->adj);
        cairo_set_source_rgba(w->crb, 0.25, 0.25, 0.05, 0.666);
        cairo_rectangle(w->crb, 0, 2, (width*state_l), height-4);
        cairo_fill(w->crb);

        double state_r = adj_get_state(self->loopMark_R->adj);
        cairo_set_source_rgba(w->crb, 0.25, 0.25, 0.05, 0.666);
        int point = (width*state_r);
        cairo_rectangle(w->crb, point, 2 , width - point, height-4);
        cairo_fill(w->crb);
        if (!self->ready) 
            show_spinning_wheel(w, nullptr);

    }

    static void drawWheel(Widget_t *w, float di, int x, int y, int radius, float s) {
        cairo_set_line_width(w->crb,10 / w->scale.ascale);
        cairo_set_line_cap (w->crb, CAIRO_LINE_CAP_ROUND);
        int i;
        const int d = 1;
        for (i=375; i<455; i++) {
            double angle = i * 0.01 * 2 * M_PI;
            double rx = radius * sin(angle);
            double ry = radius * cos(angle);
            double length_x = x - rx;
            double length_y = y + ry;
            double radius_x = x - rx * s ;
            double radius_y = y + ry * s ;
            double z = i/420.0;
            if ((int)di < d) {
                cairo_set_source_rgba(w->crb, 0.66*z, 0.66*z, 0.66*z, 0.3);
                cairo_move_to(w->crb, radius_x, radius_y);
                cairo_line_to(w->crb,length_x,length_y);
                cairo_stroke_preserve(w->crb);
            }
            di++;
            if (di>8.0) di = 0.0;
       }
    }

    static void show_spinning_wheel(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width = metrics.width;
        int height = metrics.height;
        if (!metrics.visible) return;
        SoundEditUi *self = static_cast<SoundEditUi*>(w->parent_struct);
        static const float sCent = 0.666;
        static float collectCents = 0;
        collectCents -= sCent;
        if (collectCents>8.0) collectCents = 0.0;
        else if (collectCents<0.0) collectCents = 8.0;
        self->drawWheel (w, collectCents,width*0.5, height*0.5, height*0.3, 0.98);
        cairo_stroke(w->crb);
    }

    static void draw_window(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Widget_t *p = (Widget_t*)w->parent;
        Metrics_t metrics;
        os_get_window_metrics(p, &metrics);
        if (!metrics.visible) return;
        use_bg_color_scheme(w, NORMAL_);
        cairo_paint (w->crb);
    }

/****************************************************************
                      create local widget
****************************************************************/

    static void fxbutton_mem_free(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        FileButton *filebutton = (FileButton *)w->private_struct;
        free(filebutton->last_path);
        filebutton->last_path = NULL;
        free(filebutton);
        filebutton = NULL;
    }

    Widget_t *add_xsave_file_button(Widget_t *parent, int x, int y, int width, int height,
                               const char *path, const char *filter) {
        FileButton *filebutton = (FileButton*)malloc(sizeof(FileButton));
        filebutton->path = path;
        filebutton->filter = filter;
        filebutton->last_path = NULL;
        filebutton->w = NULL;
        filebutton->is_active = false;
        Widget_t *fbutton = add_image_toggle_button(parent, "", x, y, width, height);
        fbutton->private_struct = filebutton;
        fbutton->flags |= HAS_MEM;
        fbutton->scale.gravity = CENTER;
        widget_get_png(fbutton, LDVAR(save__png));
        fbutton->func.mem_free_callback = fxbutton_mem_free;
        fbutton->func.value_changed_callback = fxbutton_callback;
        fbutton->func.dialog_callback = fxdialog_response;
        return fbutton;
    }

};

#endif

