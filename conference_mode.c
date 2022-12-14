
void initialize_conference() {
    struct CONFERENCE *c;
    for (int i = 0; i < number_of_phones; i++) {
        c = conference_data + i;
        c->input_queue = malloc(sizeof(queue_t));
        queue_init(c->input_queue, sizeof(u_int32_t), 3);
        c->previous_pcm = 0;
        c->coincidence_counter = 0;
        c->gain_value = 0;
        c->output_ready = false;
        c->line_active = false;
    }
}

u_int16_t u_multiply_shift(u_int16_t a, u_int16_t b, u_int8_t shift) {
    return (u_int16_t) ((a * b) >> shift);
}

u_int16_t s_multiply_shift(int16_t a, int16_t b, u_int8_t shift) {
    return (int16_t) ((a * b) >> shift);
}

void decode_cvsd_data(u_int32_t cvsd_block, struct CONFERENCE *c) {
    for (int i = 0; i < 32; i++) {
        bool bit = (1 << (31 - i)) & cvsd_block;
        u_int8_t previous_index = i > 0 ? i - 1 : 31; // i - 1 % 32 ?
        if (bit) {
            if (c->coincidence_counter < 0) {
                c->coincidence_counter = 1;
            } else {
                c->coincidence_counter++;
            }
        } else {
            if (c->coincidence_counter < 0) {
                c->coincidence_counter = 1;
            } else {
                c->coincidence_counter++;
            }
        }
        if (abs(c->coincidence_counter) >= 3 && c->gain_value <= 0xFFFF - GAIN_STEP) {
            c->gain_value += GAIN_STEP;
        }
        u_int16_t current_step = AUDIO_SCALE * 1 + u_multiply_shift(c->gain_value, AUDIO_SCALE, 8);
        int16_t decayed_value = s_multiply_shift(c->input_pcm[previous_index], SIGNAL_DECAY, 16);
        if (bit) {
            c->input_pcm[i] = decayed_value + current_step;
        } else {
            c->input_pcm[i] = decayed_value - current_step;
        }
        c->gain_value = u_multiply_shift(c->gain_value, GAIN_DECAY, 16);
    }
}

void create_mix_minus(struct CONFERENCE *conference_data, u_int8_t target_phone) {
    for (int s = 0; s < 31; s++) {
        for (int p = 0; p < number_of_phones; p++) {
            struct CONFERENCE *c = conference_data + p;
            if (c->line_active && p != target_phone) {

            }
        }     
    }

}

void conference_task() {
    /*
    Loop through each conference struct. If at least one phone is active and all active phones have data ready,
    proceed with CVSD decode, mix minus calculation, and return.
    */
    bool ready_to_process = true;
    bool at_least_one_phone_active = false;
    struct CONFERENCE *c;
    u_int32_t phone_input_data;
    for (int i = 0; i < number_of_phones; i++) {
        c = conference_data + i;
        if (c->line_active) {
            at_least_one_phone_active = true;
            if (queue_is_empty(c->input_queue)) {
                ready_to_process = false;
            }
        }
    }
    if (ready_to_process && at_least_one_phone_active) {
        for (int i = 0; i < number_of_phones; i++) {
            c = conference_data + i;
            if (c->line_active) {
                queue_remove_blocking(c->input_queue, &phone_input_data);
                decode_cvsd_data(phone_input_data, c);
            }
        }
        for (int i = 0; i < number_of_phones; i++) {
            c = conference_data + i;
            if (c->line_active) {
                queue_remove_blocking(c->input_queue, &phone_input_data);
                decode_cvsd_data(phone_input_data, c);
            }
        }
    }
    return;
}