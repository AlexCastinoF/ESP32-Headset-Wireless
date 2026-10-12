#include "hs_link.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failed;

static void expect(int cond, const char *name) {
    if (!cond) {
        printf("FALHOU: %s\n", name);
        g_failed++;
    }
}

int main(void) {
    expect(sizeof(hs_packet_down_t) == 250, "descida 250");
    expect(sizeof(hs_packet_up_t) == 166, "subida 166");
    expect(sizeof(hs_hid_status_t) == 32, "status 32");
    expect(sizeof(hs_hid_config_t) == 16, "config 16");
    expect(hs_estimate_one_way_ms(5, 2) == 27, "latencia equilibrio");
    expect(hs_estimate_round_trip_ms(5, 2) == 62, "ida e volta equilibrio");
    expect(hs_jitter_chunks(10, 2) == 4, "chunks do perfil estavel");
    expect(hs_battery_percent(4200) == 100, "bateria cheia");
    expect(hs_battery_percent(3200) == 0, "bateria vazia");
    expect(hs_transport_valid(5, 2, 6), "transporte valido");
    expect(!hs_transport_valid(7, 2, 6), "quadro invalido");

    int16_t tone[HS_CHUNK_FRAMES * 2];
    for (int i = 0; i < HS_CHUNK_FRAMES; i++) {
        double w = sin(2.0 * 3.141592653589793 * 1000.0 * i / 48000.0);
        int16_t s = (int16_t)(w * 12000.0);
        tone[i * 2] = s;
        tone[i * 2 + 1] = (int16_t)(-s / 2);
    }

    hs_adpcm_state_t enc_l = {0, 0};
    hs_adpcm_state_t enc_r = {0, 0};
    hs_adpcm_state_t dec_l = enc_l;
    hs_adpcm_state_t dec_r = enc_r;
    uint8_t packed[HS_CHUNK_FRAMES];
    int16_t back[HS_CHUNK_FRAMES * 2];
    hs_adpcm_encode_stereo(&enc_l, &enc_r, tone, packed, HS_CHUNK_FRAMES);
    hs_adpcm_decode_stereo(&dec_l, &dec_r, packed, back, HS_CHUNK_FRAMES);

    double err = 0;
    for (int i = 0; i < HS_CHUNK_FRAMES * 2; i++) {
        double d = (double)tone[i] - back[i];
        err += d * d;
    }
    err = sqrt(err / (HS_CHUNK_FRAMES * 2));
    printf("erro RMS do ADPCM: %.1f\n", err);
    expect(err < 1500.0, "adpcm inteligivel");

    int16_t gained[4] = {1000, -1000, 2000, -2000};
    hs_apply_gain(gained, 4, 0);
    expect(gained[0] == 1000 && gained[1] == -1000, "0 dB nao altera");

    int16_t groups[6] = {3, 3, 3, 9, 0, 0};
    int16_t dec[2];
    hs_decimate_by_3(groups, dec, 2);
    expect(dec[0] == 3 && dec[1] == 3, "media de tres");

    int16_t up_in[2] = {0, 300};
    int16_t up_out[6];
    hs_upsample_by_3(up_in, up_out, 2);
    expect(up_out[0] == 0 && up_out[3] == 300, "interpolacao");

    uint8_t frame = 99;
    uint8_t jitter = 99;
    hs_profile_apply(HS_PROFILE_GAME, &frame, &jitter);
    expect(frame == 5 && jitter == 1, "perfil jogo");

    if (g_failed) {
        printf("%d teste(s) falharam\n", g_failed);
        return 1;
    }
    printf("protocolo ok\n");
    return 0;
}
