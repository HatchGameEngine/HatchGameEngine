#ifndef FPMATH_H
#define FPMATH_H

#define FP16_MULTIPLY(a, b) (((Sint64)(a) * (b)) / 0x10000)
#define FP16_DIVIDE(a, b) (((Sint64)(a) * 0x10000) / (b))

#define FP16_TO(a) ((Sint64)((a) * 0x10000))
#define FP16_FROM(a) ((float)(a) / 0x10000)

#endif /* FPMATH_H */
