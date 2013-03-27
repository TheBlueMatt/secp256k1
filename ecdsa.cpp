#include "num.h"
#include "field.h"
#include "group.h"
#include "ecmult.h"
#include "ecdsa.h"

namespace secp256k1 {

bool ParsePubKey(GroupElemJac &elem, const unsigned char *pub, int size) {
    if (size == 33 && (pub[0] == 0x02 || pub[0] == 0x03)) {
        FieldElem x;
        x.SetBytes(pub+1);
        elem.SetCompressed(x, pub[0] == 0x03);
    } else if (size == 65 && (pub[0] == 0x04 || pub[0] == 0x06 || pub[0] == 0x07)) {
        FieldElem x,y;
        x.SetBytes(pub+1);
        y.SetBytes(pub+33);
        elem = GroupElem(x,y);
        if ((pub[0] == 0x06 || pub[0] == 0x07) && y.IsOdd() != (pub[0] == 0x07))
            return false;
    } else {
        return false;
    }
    return elem.IsValid();
}

bool Signature::Parse(const unsigned char *sig, int size) {
    if (sig[0] != 0x30) return false;
    int lenr = sig[3];
    if (5+lenr >= size) return false;
    int lens = sig[lenr+5];
    if (sig[1] != lenr+lens+4) return false;
    if (lenr+lens+6 > size) return false;
    if (sig[2] != 0x02) return false;
    if (lenr == 0) return false;
    if (sig[lenr+4] != 0x02) return false;
    if (lens == 0) return false;
    secp256k1_num_set_bin(&r, sig+4, lenr);
    secp256k1_num_set_bin(&s, sig+6+lenr, lens);
    return true;
}

bool Signature::Serialize(unsigned char *sig, int *size) {
    int lenR = (secp256k1_num_bits(&r) + 7)/8;
    if (lenR == 0 || secp256k1_num_get_bit(&r, lenR*8-1))
        lenR++;
    int lenS = (secp256k1_num_bits(&s) + 7)/8;
    if (lenS == 0 || secp256k1_num_get_bit(&s, lenS*8-1))
        lenS++;
    if (*size < 6+lenS+lenR)
        return false;
    *size = 6 + lenS + lenR;
    sig[0] = 0x30;
    sig[1] = 4 + lenS + lenR;
    sig[2] = 0x02;
    sig[3] = lenR;
    secp256k1_num_get_bin(sig+4, lenR, &r);
    sig[4+lenR] = 0x02;
    sig[5+lenR] = lenS;
    secp256k1_num_get_bin(sig+lenR+6, lenS, &s);
    return true;
}

bool Signature::RecomputeR(secp256k1_num_t &r2, const GroupElemJac &pubkey, const secp256k1_num_t &message) const {
    const GroupConstants &c = GetGroupConst();

    if (secp256k1_num_is_neg(&r) || secp256k1_num_is_neg(&s))
        return false;
    if (secp256k1_num_is_zero(&r) || secp256k1_num_is_zero(&s))
        return false;
    if (secp256k1_num_gmp(&r, &c.order) >= 0 || secp256k1_num_cmp(&s, &c.order) >= 0)
        return false;

    bool ret = false;
    secp256k1_num_t sn, u1, u2;
    secp256k1_num_init(&sn);
    secp256k1_num_init(&u1);
    secp256k1_num_init(&u2);
    secp256k1_num_mod_inverse(&sn, &s, &c.order);
    secp256k1_num_mod_mul(&u1, &sn, &message, &c.order);
    secp256k1_num_mod_mul(&u2, &sn, &r, &c.order);
    GroupElemJac pr; ECMult(pr, pubkey, u2, u1);
    if (!pr.IsInfinity()) {
        FieldElem xr; pr.GetX(xr);
        xr.Normalize();
        unsigned char xrb[32]; xr.GetBytes(xrb);
        secp256k1_num_set_bin(&r2, xrb, 32);
        secp256k1_num_mod(&r2, &r2, &c.order);
        ret = true;
    }
    secp256k1_num_free(&sn);
    secp256k1_num_free(&u1);
    secp256k1_num_free(&u2);
    return ret;
}

bool Signature::Verify(const GroupElemJac &pubkey, const secp256k1_num_t &message) const {
    secp256k1_num_t r2;
    secp256k1_num_init(&r2);
    bool ret = false;
    ret = RecomputeR(r2, pubkey, message) && secp256k1_num_cmp(&r, &r2) == 0;
    secp256k1_num_free(&r2);
    return ret;
}

bool Signature::Sign(const secp256k1_num_t &seckey, const secp256k1_num_t &message, const secp256k1_num_t &nonce) {
    const GroupConstants &c = GetGroupConst();

    GroupElemJac rp;
    ECMultBase(rp, nonce);
    FieldElem rx;
    rp.GetX(rx);
    unsigned char b[32];
    rx.Normalize();
    rx.GetBytes(b);
    r.SetBytes(b, 32);
    r.SetMod(r, c.order);
    secp256k1_num_t n;
    n.SetModMul(r, seckey, c.order);
    n.SetAdd(message, n);
    s.SetModInverse(nonce, c.order);
    s.SetModMul(s, n, c.order);
    if (s.IsZero())
        return false;
    if (s.IsOdd())
        s.SetSub(c.order, s);
    return true;
}

void Signature::SetRS(const secp256k1_num_t &rin, const secp256k1_num_t &sin) {
    r = rin;
    s = sin;
}

std::string Signature::ToString() const {
    return "(" + r.ToString() + "," + s.ToString() + ")";
}

}
