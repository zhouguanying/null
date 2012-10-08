#ifndef __MAIL_ALARM__H__
#define __MAIL_ALARM__H__

extern struct __attach_data_list_head attach_data_list_head;
extern char mailserver[64];
extern char receiver[64];
extern char sender[64];
extern char sendername[64];
extern char senderpswd[64];


void init_mail_attatch_data_list(char *mailbox);
int add_image_to_mail_attatch_list_no_block(char * image, int size);
int mail_alarm_thread();

#endif
