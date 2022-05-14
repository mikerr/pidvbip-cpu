#ifndef _OSD_H
#define _OSD_H

#include <pthread.h>

/* The various OSD screens */
#define OSD_NONE 0
#define OSD_INFO 1
#define OSD_NEWCHANNEL 2
#define OSD_CHANNELLIST 3


struct osd_t {
  int img;
  int img_blank;
  int display_width;
  int display_height;
  pthread_mutex_t osd_mutex;
  int video_blanked;

  int osd_state;  /* Which OSD screen we are displaying (or OSD_NONE) */

  /* State of various screens */
  double osd_cleartime;
  time_t last_now;
  uint32_t event;
  uint32_t  nextEvent;
  /* state of channel list */
  int channellist_start_channel;
  int channellist_selected_channel;
  int channellist_selected_pos; 
  int channellist_prev_selected_pos;
  int channellist_prev_selected_channel; 
};

void osd_init(struct osd_t* osd);
void osd_done(struct osd_t* osd);
void osd_show_info(struct osd_t* osd, int channel_id, int timeout);
void osd_show_newchannel(struct osd_t* osd, int channel);
void osd_update(struct osd_t* osd, int channel_id);
void osd_clear(struct osd_t* osd);
void osd_clear_newchannel(struct osd_t* osd);
void osd_blank_video(struct osd_t* osd, int on_off);

int osd_process_key(struct osd_t* osd, int c);
void osd_channellist_display(struct osd_t* osd);
void osd_recordings(struct osd_t* osd);

double get_time(void); /* Get time of day in ms */

#endif
