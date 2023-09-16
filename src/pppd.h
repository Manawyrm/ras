#pragma once

struct rfc1662_vars {
    int deframed_bytes;
    unsigned char escape;
};

int start_pppd(int *fd, int *pppd);
int32_t pppd_rfc1662_encode(const uint8_t *raw_in, uint32_t raw_len, uint8_t *hdlc_out);
int pppd_rfc1662_decode(struct rfc1662_vars *state, const uint8_t *src,
                        int slen, int *count, uint8_t *dst, int dsize);