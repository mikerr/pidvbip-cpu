
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <bcm_host.h>
#include "osd.h"
#include "channels.h"
#include "events.h"
#include "utils.h"

#include <FTGL/ftgl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GL/gl.h>

/* constants for channellist */
#define CHANNELLIST_MIDDLE 0
#define CHANNELLIST_TOP 1
#define CHANNELLIST_BOTTOM 2
#define CHANNELLIST_TEXTSIZE 40
#define CHANNELLIST_UP 1
#define CHANNELLIST_DOWN 2

// global variables declarations

static int device;
static drmModeRes *resources;
static drmModeConnector *connector;
static uint32_t connector_id;
static drmModeEncoder *encoder;
static drmModeModeInfo mode_info;
static drmModeCrtc *crtc;
static struct gbm_device *gbm_device;
static EGLDisplay display;
static EGLContext context;
static struct gbm_surface *gbm_surface;
static EGLSurface egl_surface;
       EGLConfig config;
       EGLint num_config;
       EGLint count=0;
       EGLConfig *configs;
       int config_index;
       int i;
       
static struct gbm_bo *previous_bo = NULL;
static uint32_t previous_fb;       

static EGLint attributes[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
		};

static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

struct gbm_bo *bo;	
uint32_t handle;
uint32_t pitch;
uint32_t fb;
uint64_t modifier;

FTGLfont *font;

static void update_screen () {

eglSwapBuffers (display, egl_surface);
bo = gbm_surface_lock_front_buffer (gbm_surface);
handle = gbm_bo_get_handle (bo).u32;
pitch = gbm_bo_get_stride (bo);
drmModeAddFB (device, mode_info.hdisplay, mode_info.vdisplay, 24, 32, pitch, handle, &fb);
drmModeSetCrtc (device, crtc->crtc_id, fb, 0, 0, &connector_id, 1, &mode_info);
if (previous_bo) {
  drmModeRmFB (device, previous_fb);
  gbm_surface_release_buffer (gbm_surface, previous_bo);
  }
previous_bo = bo;
previous_fb = fb;
}

static drmModeConnector *find_connector (drmModeRes *resources) {

for (i=0; i<resources->count_connectors; i++) {
  drmModeConnector *connector = drmModeGetConnector (device, resources->connectors[i]);
  if (connector->connection == DRM_MODE_CONNECTED) {return connector;}
  drmModeFreeConnector (connector);
  }
return NULL; // if no connector found
}

static drmModeEncoder *find_encoder (drmModeRes *resources, drmModeConnector *connector) {

if (connector->encoder_id) {return drmModeGetEncoder (device, connector->encoder_id);}
return NULL; // if no encoder found
}

static int match_config_to_visual(EGLDisplay egl_display, EGLint visual_id, EGLConfig *configs, int count) {

EGLint id;
for (i = 0; i < count; ++i) {
  if (!eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID,&id)) continue;
  if (id == visual_id) return i;
  }
return -1;
}

void drm_init () {

device = open ("/dev/dri/card1", O_RDWR);
resources = drmModeGetResources (device);
connector = find_connector (resources);
connector_id = connector->connector_id;
mode_info = connector->modes[0];
encoder = find_encoder (resources, connector);
crtc = drmModeGetCrtc (device, encoder->crtc_id);
drmModeFreeEncoder (encoder);
drmModeFreeConnector (connector);
drmModeFreeResources (resources);
gbm_device = gbm_create_device (device);
gbm_surface = gbm_surface_create (gbm_device, mode_info.hdisplay, mode_info.vdisplay, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);
display = eglGetDisplay (gbm_device);
eglInitialize (display, NULL ,NULL);
eglBindAPI (EGL_OPENGL_API);
eglGetConfigs(display, NULL, 0, &count);
configs = malloc(count * sizeof *configs);
eglChooseConfig (display, attributes, configs, count, &num_config);
config_index = match_config_to_visual(display,GBM_FORMAT_XRGB8888,configs,num_config);
context = eglCreateContext (display, configs[config_index], EGL_NO_CONTEXT, context_attribs);
egl_surface = eglCreateWindowSurface (display, configs[config_index], gbm_surface, NULL);
free(configs);
eglMakeCurrent (display, egl_surface, egl_surface, context);
}

void drm_cleanup() {
	
drmModeSetCrtc (device, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &connector_id, 1, &crtc->mode);
drmModeFreeCrtc (crtc);
if (previous_bo) {
  drmModeRmFB (device, previous_fb);
  gbm_surface_release_buffer (gbm_surface, previous_bo);
  }
eglDestroySurface (display, egl_surface);
gbm_surface_destroy (gbm_surface);
eglDestroyContext (display, context);
eglTerminate (display);
gbm_device_destroy (gbm_device);

close (device);
}

void setPos (int x,int y) {
	float fx,fy;
  
	// normalize 0,0 to -1,-1
	// normalize 200,200 to 1,1
	
  	fy = (200 - y) / 100.0f;
  	fy--;

  	fx = x / 100.0f;
  	fx--;
  	glRasterPos2f(fx,fy);
}

void printText(const char *text,int pointsize) {
  ftglSetFontFaceSize(font, pointsize, pointsize);
  ftglRenderFont(font, text, FTGL_RENDER_ALL);
}

void printMultiline(char *text, int y){
  	int linepos = 0;
  	char line[100];
  	for (int i=0;i<strlen(text);i++) {
	  	line[linepos++] = text[i];
	  	if (text[i] == ' ' && linepos > 90) {
  		  	line[linepos] = 0;
			linepos = 0;
  		  	printText(line,16);
		  	y = y + 10;
  		  	setPos(10,y);
	  	}
  	}
  	line[linepos] = 0;
  	printText(line,16);
  }

int GRAPHICS_RGBA32 (int r,int g,int b,int a) { return 0; }
static int CHANNELLIST_NUM_CHANNELS = 12;

static void utf8decode(char* str, char* r)
{
  int x,y,z,ud;
  char* p = str;

  while ((z = *p++)) {
    if (z < 128) {
      *r++ = z;
    } else {
      y=*p++;
      if (y==0) { *r=0 ; return; } // ERROR
      if (z < 224) {
        ud=(z-192)*64 + (y-128);
      } else {
        x=*p++;
        if (x==0) { *r=0 ; return; } // ERROR
        ud=(z-224)*4096 + (y-128)*64 + (x-128);
      }

      if (ud < 256) {
        *r++ = ud;
      } else {
        /* Transliterate some common characters */
        switch (ud) {  
          /* Add more mappings here if required  */
          case 0x201c:                              // quotedblleft
          case 0x201d: *r++ = '"';  break;          // quotedblright
          case 0x2018:                              // quoteleft
          case 0x2019: *r++ = '\''; break;          // quoteright
          case 0x2013:                              // en dash
          case 0x2014: *r++ = '-'; break;           // em dash
          case 0x20ac: *r++ = 0xa4; break;          // euro
          case 0x27a2:                              // square
          case 0x25ca:                              // diamond
          case 0xf076:
          case 0xf0a7:
          case 0xf0b7:
          case 0x2022: *r++ = '\267'; break;        // bullet ("MIDDLE DOT" in iso-8859-1)
          case 0x2026: fprintf(stdout,"..."); break;  // ellipsis

          default:     //fprintf(stderr,"Unknown character %04x (UTF-8 is %02x %02x %02x)\n",ud,z,y,x);
          *r++ = ' ';   
           break;
        }
      }
    }
  }

  *r++ = 0;
  return;
}

void osd_init(struct osd_t* osd)
{
   drm_init();

   /* Create a pixmap font from a TrueType file. */
   font = ftglCreatePixmapFont("Vera.ttf");

   if(!font) return;

   glClear(GL_COLOR_BUFFER_BIT);
   setPos(50,120);
   printText("piDVBip",144);
   update_screen();

   osd->video_blanked = 0;

   pthread_mutex_init(&osd->osd_mutex,NULL);
}

void osd_blank_video(struct osd_t* osd, int on_off)
{
  pthread_mutex_lock(&osd->osd_mutex);
  glClear(GL_COLOR_BUFFER_BIT);
  update_screen();
  pthread_mutex_unlock(&osd->osd_mutex);
}

static void osd_show_channelname(struct osd_t* osd, const char *text)
{
  setPos(0,5);
  printText(text,16);
}

static void osd_show_eventinfo(struct osd_t* osd, struct event_t* event)
{
  char str[64];
  struct tm start_time;
  struct tm stop_time;
  int duration;

  if (event==NULL)
    return;

  localtime_r((time_t*)&event->start,&start_time);
  localtime_r((time_t*)&event->stop,&stop_time);
  duration = event->stop - event->start;

  // programme title
  setPos(10,150);
  printText(event->title,16);

  // programme start and stop time
  snprintf(str,sizeof(str),"%02d:%02d - %02d:%02d",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min);
  setPos(10,160);
  printText(str,16);

  // programme duration
  snprintf(str,sizeof(str),"%dh %02dm",duration/3600,(duration%3600)/60);
  setPos(50,160);
  printText(str,16);

  // description ( multi line)
  setPos(10,170);
  printMultiline(event->description,170);

}
void osd_show_time(struct osd_t* osd)
{
  struct tm now_tm;
  time_t now;
  char str[32];

  now = time(NULL);
  localtime_r(&now,&now_tm);

  snprintf(str,sizeof(str),"%02d:%02d",now_tm.tm_hour,now_tm.tm_min);

  setPos(190,5);
  printText(str,16);
}

void osd_show_info(struct osd_t* osd, int channel_id, int timeout)
{
  char str[128];
  struct event_t* event = event_copy(channels_geteventid(channel_id));

  event_dump(event);

  snprintf(str,sizeof(str),"%03d - %s",channels_getlcn(channel_id),channels_getname(channel_id));

  pthread_mutex_lock(&osd->osd_mutex);

  glClear(GL_COLOR_BUFFER_BIT);

  osd_show_channelname(osd,str);
  osd_show_time(osd);
  osd_show_eventinfo(osd,event);

  update_screen();

  pthread_mutex_unlock(&osd->osd_mutex);

  event_free(event);

  osd->osd_state = OSD_INFO;
  osd->osd_cleartime = get_time() + timeout;
}

void osd_show_newchannel(struct osd_t* osd, int channel)
{
  char str[128];

  snprintf(str,sizeof(str),"%d",channel);
  if (channel < 1000) {
    str[4] = 0;
    str[3] = '-';
    if (channel < 100) {
      str[2] = '-';
      if (channel < 10) {
        str[1] = '-';
      }
    }
  }
  fprintf(stderr,"New channel = %s\n",str);
  pthread_mutex_lock(&osd->osd_mutex);
  osd_show_channelname(osd,str);
  update_screen();
  pthread_mutex_unlock(&osd->osd_mutex);
}

void osd_clear_newchannel(struct osd_t* osd)
{
  pthread_mutex_lock(&osd->osd_mutex);

  glClear(GL_COLOR_BUFFER_BIT);
  update_screen();

  pthread_mutex_unlock(&osd->osd_mutex);
}

void osd_clear(struct osd_t* osd)
{
  pthread_mutex_lock(&osd->osd_mutex);

  glClear(GL_COLOR_BUFFER_BIT);
  glColor3f(1,1,1);
  update_screen();

  pthread_mutex_unlock(&osd->osd_mutex);

  fprintf(stderr,"Clearing OSD...\n");
  osd->osd_state = OSD_NONE;
  osd->osd_cleartime = 0;
}

void osd_channellist_show_epg_line(struct osd_t* osd, int channel_id, int y) {
  
  osd->event = channels_geteventid(channel_id);
  osd->nextEvent = channels_getnexteventid(channel_id);

  struct event_t* event = event_copy(osd->event);
  struct event_t* nextEvent = event_copy(osd->nextEvent);

  if (event == NULL) return;

  setPos(50,y);
  printText(event->title,16);

  setPos(100,y);
  printText(nextEvent->title,16);

  event_free(event);
  event_free(nextEvent);
}

void osd_channellist_show_epg(struct osd_t* osd, int channel_id)
{
  char str[128];
  struct tm start_time;
  struct tm stop_time;
  char* iso_text = NULL;
  
  osd->event = channels_geteventid(channel_id);
  osd->nextEvent = channels_getnexteventid(channel_id);

  struct event_t* event = event_copy(osd->event);
  struct event_t* nextEvent = event_copy(osd->nextEvent);

  snprintf(str, sizeof(str),"%s",channels_getname(channel_id));
  setPos(10,5);
  printText(str,16);

  if (event == NULL) return;

  /* Start/stop time - current event */
  localtime_r((time_t*)&event->start, &start_time);
  localtime_r((time_t*)&event->stop, &stop_time);
  if (event->title) {
    iso_text = malloc(strlen(event->title)+1);
    utf8decode(event->title, iso_text);
  }

  snprintf(str, sizeof(str),"%02d:%02d - %02d:%02d %s",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min, iso_text);
  setPos(10,20);
  printText(str,16);
  free(iso_text);

  if (event->description) {
    char* iso_text = malloc(strlen(event->description)+1);
    utf8decode(event->description,iso_text);
    setPos(10,30);
    printMultiline(iso_text,30);
    free(iso_text);
  }

  event_free(event);
  event_free(nextEvent);
}

void osd_channellist_display_channels(struct osd_t* osd)
{
  int num_channels, num_display, id;
  int selected = 0;
  char str[60];
  uint32_t x = 0;
  uint32_t y = 60;
  char* iso_text = NULL; 
  int first_channel;
  
  num_channels = channels_getcount();
  first_channel = channels_getfirst();
  
  if (num_channels > 0) {
    // display max CHANNELLIST_NUM_CHANNELS channels
    CHANNELLIST_NUM_CHANNELS = 14;
    num_display = num_channels > CHANNELLIST_NUM_CHANNELS ? CHANNELLIST_NUM_CHANNELS : num_channels;
    id = osd->channellist_start_channel;
    
    for (int i = 0; i < num_display; i++) {      
      if (id == osd->channellist_selected_channel) {
        selected = 1;
        osd->channellist_selected_pos = i;
        glColor3f(0,1,0);
      }
      else {
        selected = 0;
        glColor3f(1,1,1);
      } 
            
      snprintf(str, sizeof(str), "%d %s", channels_getlcn(id), channels_getname(id)); 
      iso_text = malloc(strlen(str) + 1);
      utf8decode(str, iso_text);        

      setPos(x,y);
      printText(iso_text,16);
                                            
      //fprintf(stderr, "%d %s %d\n", id, str, selected);  
      osd_channellist_show_epg_line(osd, id, y);

      y += 10;
      free(iso_text);     
      id = channels_getnext(id);   
      if (id == first_channel) {
        if (selected) {
          //osd->channellist_end = 1;
        }  
        break;
      }
    }
    osd_channellist_show_epg(osd, osd->channellist_selected_channel);    
  }
}

/*
 * Displays the channellist window
 */
void osd_channellist_display(struct osd_t* osd)
{   
  
  pthread_mutex_lock(&osd->osd_mutex);

  glClear(GL_COLOR_BUFFER_BIT);
  osd_channellist_display_channels(osd);
  osd_show_time(osd);
  update_screen();

  pthread_mutex_unlock(&osd->osd_mutex);
  osd->osd_state = OSD_CHANNELLIST;  
}

/*
 * Returns CHANNELLIST_TOP, CHANNELLIST_MIDDLE, or CHANNELLIST_BOTTOM depending 
 * of the selected channel position in the channel list
 */
int osd_channellist_selected_position(struct osd_t* osd)
{
  if (osd->channellist_selected_pos == CHANNELLIST_NUM_CHANNELS - 1 ||
      osd->channellist_selected_channel == channels_getlast() ) {
    return CHANNELLIST_BOTTOM;
  } 
  else if (osd->channellist_selected_pos == 0) {
    return CHANNELLIST_TOP;
  }
  return CHANNELLIST_MIDDLE;
}

void osd_update(struct osd_t* osd, int channel_id)
{
  if ((osd->osd_cleartime) && (get_time() > osd->osd_cleartime)) {
    osd_clear(osd);
    return;
  }
}

int osd_process_key(struct osd_t* osd, int c) {
  int id;
  int i;
  int num_ch_dsp = CHANNELLIST_NUM_CHANNELS;

  if (osd->osd_state == OSD_NONE) { 
    return c;
  }
  
  if (osd->osd_state == OSD_CHANNELLIST) {
    switch (c) {
      case 'd':
        if (osd_channellist_selected_position(osd) == CHANNELLIST_BOTTOM) {
          // On bottom
          osd->channellist_selected_channel = channels_getnext(osd->channellist_selected_channel);   
          osd->channellist_start_channel = osd->channellist_selected_channel;
          osd_channellist_display(osd);          
  	  update_screen();
        }
        else {
          osd->channellist_prev_selected_channel = osd->channellist_selected_channel;
          osd->channellist_selected_channel = channels_getnext(osd->channellist_selected_channel);  
          //osd_channellist_update_channels(osd, CHANNELLIST_DOWN);
          osd_channellist_display(osd);          
  	  //update_screen();
        }     
        break;
      case 'u':
        if (osd_channellist_selected_position(osd) == CHANNELLIST_TOP) {
          // On top
          num_ch_dsp = CHANNELLIST_NUM_CHANNELS; 
          if (osd->channellist_selected_channel == channels_getfirst() ) {
            num_ch_dsp = channels_getcount() % CHANNELLIST_NUM_CHANNELS;
          }  
          osd->channellist_selected_channel = channels_getprev(osd->channellist_selected_channel);
          for (i = 0; i < num_ch_dsp; i++) {
            osd->channellist_start_channel = channels_getprev(osd->channellist_start_channel);
          }  
          osd_channellist_display(osd);          
        }
        else {
          osd->channellist_prev_selected_channel = osd->channellist_selected_channel;
          osd->channellist_selected_channel = channels_getprev(osd->channellist_selected_channel);  
          //osd_channellist_update_channels(osd, CHANNELLIST_UP);        
          osd_channellist_display(osd);          
        }      
        break;  
      case '.':
        // Next page
        id = osd->channellist_start_channel;
        for (i = 0; i < 12; i++) {
          id = channels_getnext(id); 
          if (id == channels_getfirst() ) {
            break;
          }  
        }
        osd->channellist_selected_channel = id;
        osd->channellist_start_channel = id;
        osd_channellist_display(osd);
        break;
      case ',':
        // Prev page
        if (osd->channellist_start_channel == channels_getfirst() ) {
          num_ch_dsp = channels_getcount() % CHANNELLIST_NUM_CHANNELS;
        }

        id = osd->channellist_start_channel;        
        for (i = 0; i < num_ch_dsp; i++) {
          id = channels_getprev(id); 
        }
        osd->channellist_selected_channel = id;
        osd->channellist_start_channel = id;
        osd_channellist_display(osd);        
        break;
      case 'i':
        osd_clear(osd);
        return c;
        break;
      default:
        return c;
    }
    return -1;
  }
  return c;
}

