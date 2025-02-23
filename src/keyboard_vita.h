#ifndef __KEYBOARD_VITA_H__
#define __KEYBOARD_VITA_H__

#define IME_DIALOG_RESULT_NONE 0
#define IME_DIALOG_RESULT_RUNNING 1
#define IME_DIALOG_RESULT_FINISHED 2
#define IME_DIALOG_RESULT_CANCELED 3

void initAppUtils();
int initImeDialog(char *title, char *initial_text, int max_text_length, int type, int option);
uint16_t *getImeDialogInputTextUTF16();
uint8_t *getImeDialogInputTextUTF8();
int isImeDialogRunning();
int updateImeDialog();
char *keyboard_vita_get(char *title, int maxLen);

#endif
