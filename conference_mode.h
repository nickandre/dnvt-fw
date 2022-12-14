
struct CONFERENCE {
    queue_t *input_queue;
    u_int16_t previous_pcm;
    int8_t coincidence_counter;
    u_int16_t gain_value;
    int8_t output_coincidence_counter;
    u_int16_t output_gain_value;
    int16_t input_pcm[32];
    u_int32_t output_data;
    bool output_ready;
    bool line_active;
};