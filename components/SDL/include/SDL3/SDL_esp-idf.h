/*
    ESP-IDF specific headers for direct access to some functions.
*/
#ifdef CONFIG_IDF_TARGET_ESP32P4
// PPA helper function to scale image directly before streming it to HW
void set_scale_factor(int factor, float factor_float);
#endif