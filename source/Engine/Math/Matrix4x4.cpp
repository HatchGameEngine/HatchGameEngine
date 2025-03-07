#include <Engine/Math/Matrix4x4.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Math/Math.h>

#include <math.h>
#include <string.h>

#define EPSILON 0.000001

Matrix4x4* Matrix4x4::Create() {
	Matrix4x4* mat4 = new Matrix4x4;
	Matrix4x4::Identity(mat4);
	return mat4;
}
void Matrix4x4::Identity(Matrix4x4* mat4) {
	mat4->Values[0] = 1.0f;
	mat4->Values[1] = 0.0f;
	mat4->Values[2] = 0.0f;
	mat4->Values[3] = 0.0f;
	mat4->Values[4] = 0.0f;
	mat4->Values[5] = 1.0f;
	mat4->Values[6] = 0.0f;
	mat4->Values[7] = 0.0f;
	mat4->Values[8] = 0.0f;
	mat4->Values[9] = 0.0f;
	mat4->Values[10] = 1.0f;
	mat4->Values[11] = 0.0f;
	mat4->Values[12] = 0.0f;
	mat4->Values[13] = 0.0f;
	mat4->Values[14] = 0.0f;
	mat4->Values[15] = 1.0f;
}

void Matrix4x4::Perspective(Matrix4x4* out, float fovy, float aspect, float near, float far) {
	float f = 1.0f / tan(fovy / 2.0f);
	float nf = 1.0f / (near - far);
	out->Values[0] = f / aspect;
	out->Values[1] = 0.0f;
	out->Values[2] = 0.0f;
	out->Values[3] = 0.0f;
	out->Values[4] = 0.0f;
	out->Values[5] = f;
	out->Values[6] = 0.0f;
	out->Values[7] = 0.0f;
	out->Values[8] = 0.0f;
	out->Values[9] = 0.0f;
	out->Values[10] = (far + near) * nf;
	out->Values[11] = -1.0f;
	out->Values[12] = 0.0f;
	out->Values[13] = 0.0f;
	out->Values[14] = 2.0f * far * near * nf;
	out->Values[15] = 0.0f;
}
void Matrix4x4::Ortho(Matrix4x4* out,
	float left,
	float right,
	float bottom,
	float top,
	float near,
	float far) {
	float lr = 1.0f / (left - right);
	float bt = 1.0f / (bottom - top);
	float nf = 1.0f / (near - far);
	out->Values[0] = -2.0f * lr;
	out->Values[1] = 0.0f;
	out->Values[2] = 0.0f;
	out->Values[3] = 0.0f;
	out->Values[4] = 0.0f;
	out->Values[5] = -2.0f * bt;
	out->Values[6] = 0.0f;
	out->Values[7] = 0.0f;
	out->Values[8] = 0.0f;
	out->Values[9] = 0.0f;
	out->Values[10] = 2.0f * nf;
	out->Values[11] = 0.0f;
	out->Values[12] = (left + right) * lr;
	out->Values[13] = (top + bottom) * bt;
	out->Values[14] = (far + near) * nf;
	out->Values[15] = 1.0f;
}

void Matrix4x4::Copy(Matrix4x4* out, Matrix4x4* a) {
	memcpy(&out->Values[0], &a->Values[0], 16 * sizeof(float));
}
bool Matrix4x4::Equals(Matrix4x4* a, Matrix4x4* b) {
	if (!a && !b) {
		return true;
	}
	if (!a || !b) {
		return false;
	}

	return memcmp(&a->Values[0], &b->Values[0], 16 * sizeof(float)) == 0;
}

void Matrix4x4::Multiply(Matrix4x4* out, Matrix4x4* a, Matrix4x4* b) {
	float a00 = a->Values[0], a01 = a->Values[1], a02 = a->Values[2], a03 = a->Values[3];
	float a10 = a->Values[4], a11 = a->Values[5], a12 = a->Values[6], a13 = a->Values[7];
	float a20 = a->Values[8], a21 = a->Values[9], a22 = a->Values[10], a23 = a->Values[11];
	float a30 = a->Values[12], a31 = a->Values[13], a32 = a->Values[14], a33 = a->Values[15];

	// Cache only the current line of the second matrix
	float b0 = b->Values[0], b1 = b->Values[1], b2 = b->Values[2], b3 = b->Values[3];
	out->Values[0] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
	out->Values[1] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
	out->Values[2] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
	out->Values[3] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

	b0 = b->Values[4];
	b1 = b->Values[5];
	b2 = b->Values[6];
	b3 = b->Values[7];
	out->Values[4] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
	out->Values[5] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
	out->Values[6] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
	out->Values[7] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

	b0 = b->Values[8];
	b1 = b->Values[9];
	b2 = b->Values[10];
	b3 = b->Values[11];
	out->Values[8] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
	out->Values[9] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
	out->Values[10] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
	out->Values[11] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

	b0 = b->Values[12];
	b1 = b->Values[13];
	b2 = b->Values[14];
	b3 = b->Values[15];
	out->Values[12] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
	out->Values[13] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
	out->Values[14] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
	out->Values[15] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;
}
void Matrix4x4::Multiply(Matrix4x4* mat, float* a) {
	float a0 = a[0];
	float a1 = a[1];
	float a2 = a[2];
	float a3 = a[3];

	a[0] = mat->Values[0] * a0 + mat->Values[4] * a1 + mat->Values[8] * a2 +
		mat->Values[12] * a3;
	a[1] = mat->Values[1] * a0 + mat->Values[5] * a1 + mat->Values[9] * a2 +
		mat->Values[13] * a3;
	a[2] = mat->Values[2] * a0 + mat->Values[6] * a1 + mat->Values[10] * a2 +
		mat->Values[14] * a3;
	a[3] = mat->Values[3] * a0 + mat->Values[7] * a1 + mat->Values[11] * a2 +
		mat->Values[15] * a3;
}

void Matrix4x4::Translate(Matrix4x4* out, Matrix4x4* a, float x, float y, float z) {
	float a00, a01, a02, a03;
	float a10, a11, a12, a13;
	float a20, a21, a22, a23;

	if (a == out) {
		out->Values[12] =
			a->Values[0] * x + a->Values[4] * y + a->Values[8] * z + a->Values[12];
		out->Values[13] =
			a->Values[1] * x + a->Values[5] * y + a->Values[9] * z + a->Values[13];
		out->Values[14] =
			a->Values[2] * x + a->Values[6] * y + a->Values[10] * z + a->Values[14];
		out->Values[15] =
			a->Values[3] * x + a->Values[7] * y + a->Values[11] * z + a->Values[15];
	}
	else {
		a00 = a->Values[0];
		a01 = a->Values[1];
		a02 = a->Values[2];
		a03 = a->Values[3];
		a10 = a->Values[4];
		a11 = a->Values[5];
		a12 = a->Values[6];
		a13 = a->Values[7];
		a20 = a->Values[8];
		a21 = a->Values[9];
		a22 = a->Values[10];
		a23 = a->Values[11];

		// Copy the first 12 values
		memcpy(&out->Values[0], &a->Values[0], 12 * sizeof(float));

		out->Values[12] = a00 * x + a10 * y + a20 * z + a->Values[12];
		out->Values[13] = a01 * x + a11 * y + a21 * z + a->Values[13];
		out->Values[14] = a02 * x + a12 * y + a22 * z + a->Values[14];
		out->Values[15] = a03 * x + a13 * y + a23 * z + a->Values[15];
	}
}
void Matrix4x4::Scale(Matrix4x4* out, Matrix4x4* a, float x, float y, float z) {
	out->Values[0] = a->Values[0] * x;
	out->Values[1] = a->Values[1] * x;
	out->Values[2] = a->Values[2] * x;
	out->Values[3] = a->Values[3] * x;
	out->Values[4] = a->Values[4] * y;
	out->Values[5] = a->Values[5] * y;
	out->Values[6] = a->Values[6] * y;
	out->Values[7] = a->Values[7] * y;
	out->Values[8] = a->Values[8] * z;
	out->Values[9] = a->Values[9] * z;
	out->Values[10] = a->Values[10] * z;
	out->Values[11] = a->Values[11] * z;
	out->Values[12] = a->Values[12];
	out->Values[13] = a->Values[13];
	out->Values[14] = a->Values[14];
	out->Values[15] = a->Values[15];
}
void Matrix4x4::Rotate(Matrix4x4* out, Matrix4x4* a, float rad, float x, float y, float z) {
	float len = sqrt(x * x + y * y + z * z);
	float s, c, t;
	float a00, a01, a02, a03;
	float a10, a11, a12, a13;
	float a20, a21, a22, a23;

	float b00, b01, b02;
	float b10, b11, b12;
	float b20, b21, b22;

	if (abs(len) < EPSILON) {
		return;
	}

	len = 1.0f / len;
	x *= len;
	y *= len;
	z *= len;

	s = sin(rad);
	c = cos(rad);
	t = 1.0f - c;

	a00 = a->Values[0];
	a01 = a->Values[1];
	a02 = a->Values[2];
	a03 = a->Values[3];
	a10 = a->Values[4];
	a11 = a->Values[5];
	a12 = a->Values[6];
	a13 = a->Values[7];
	a20 = a->Values[8];
	a21 = a->Values[9];
	a22 = a->Values[10];
	a23 = a->Values[11];

	// Construct the elements of the rotation matrix
	b00 = x * x * t + c;
	b01 = y * x * t + z * s;
	b02 = z * x * t - y * s;
	b10 = x * y * t - z * s;
	b11 = y * y * t + c;
	b12 = z * y * t + x * s;
	b20 = x * z * t + y * s;
	b21 = y * z * t - x * s;
	b22 = z * z * t + c;

	// Perform rotation-specific matrix multiplication
	out->Values[0] = a00 * b00 + a10 * b01 + a20 * b02;
	out->Values[1] = a01 * b00 + a11 * b01 + a21 * b02;
	out->Values[2] = a02 * b00 + a12 * b01 + a22 * b02;
	out->Values[3] = a03 * b00 + a13 * b01 + a23 * b02;
	out->Values[4] = a00 * b10 + a10 * b11 + a20 * b12;
	out->Values[5] = a01 * b10 + a11 * b11 + a21 * b12;
	out->Values[6] = a02 * b10 + a12 * b11 + a22 * b12;
	out->Values[7] = a03 * b10 + a13 * b11 + a23 * b12;
	out->Values[8] = a00 * b20 + a10 * b21 + a20 * b22;
	out->Values[9] = a01 * b20 + a11 * b21 + a21 * b22;
	out->Values[10] = a02 * b20 + a12 * b21 + a22 * b22;
	out->Values[11] = a03 * b20 + a13 * b21 + a23 * b22;

	if (a != out) {
		// If the source and destination differ, copy the
		// unchanged last row
		out->Values[12] = a->Values[12];
		out->Values[13] = a->Values[13];
		out->Values[14] = a->Values[14];
		out->Values[15] = a->Values[15];
	}
}
void Matrix4x4::IdentityScale(Matrix4x4* out, float x, float y, float z) {
	Matrix4x4::Identity(out);

	out->Values[0] = x;
	out->Values[5] = y;
	out->Values[10] = z;
}
void Matrix4x4::IdentityRotationX(Matrix4x4* out, float x) {
	Matrix4x4::Identity(out);

	float sinX = Math::Sin(x);
	float cosX = Math::Cos(x);
	out->Values[4] = cosX;
	out->Values[5] = sinX;
	out->Values[8] = -sinX;
	out->Values[9] = cosX;
}
void Matrix4x4::IdentityRotationY(Matrix4x4* out, float y) {
	Matrix4x4::Identity(out);

	float sinY = Math::Sin(y);
	float cosY = Math::Cos(y);
	out->Values[0] = cosY;
	out->Values[2] = sinY;
	out->Values[8] = -sinY;
	out->Values[10] = cosY;
}
void Matrix4x4::IdentityRotationZ(Matrix4x4* out, float z) {
	Matrix4x4::Identity(out);

	float sinZ = Math::Sin(z);
	float cosZ = Math::Cos(z);

	out->Values[0] = cosZ;
	out->Values[1] = sinZ;
	out->Values[4] = -sinZ;
	out->Values[5] = cosZ;
}
void Matrix4x4::IdentityRotationXYZ(Matrix4x4* out, float x, float y, float z) {
	float sinX = Math::Sin(x);
	float cosX = Math::Cos(x);
	float sinY = Math::Sin(y);
	float cosY = Math::Cos(y);
	float sinZ = Math::Sin(z);
	float cosZ = Math::Cos(z);
	float sinXY = sinX * sinY;
	out->Values[9] = sinX;
	out->Values[12] = 0.f;
	out->Values[13] = 0.f;
	out->Values[0] = (cosY * cosZ) + (sinZ * sinXY);
	out->Values[4] = (cosY * sinZ) - (cosZ * sinXY);
	out->Values[8] = cosX * sinY;
	out->Values[1] = -(cosX * sinZ);
	out->Values[5] = cosX * cosZ;

	float sincosXY = sinX * cosY;
	out->Values[2] = (sinZ * sincosXY) - (sinY * cosZ);
	out->Values[6] = (-(sinZ * sinY)) - (cosZ * sincosXY);
	out->Values[14] = 0.f;
	out->Values[3] = 0.f;
	out->Values[7] = 0.f;
	out->Values[10] = cosX * cosY;
	out->Values[11] = 0.f;
	out->Values[15] = 1.f;
}
void Matrix4x4::Transpose(Matrix4x4* out) {
	Matrix4x4 transposed;

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			transposed.Values[(i * 4) + j] = out->Values[(j * 4) + i];
		}
	}

	Matrix4x4::Copy(out, &transposed);
}
void Matrix4x4::LookAt(Matrix4x4* out,
	float eyex,
	float eyey,
	float eyez,
	float centerx,
	float centery,
	float centerz,
	float upx,
	float upy,
	float upz) {
	float x0, x1, x2, y0, y1, y2, z0, z1, z2, len;
	if (Math::Abs(eyex - centerx) < EPSILON && Math::Abs(eyey - centery) < EPSILON &&
		Math::Abs(eyez - centerz) < EPSILON) {
		Matrix4x4::Identity(out);
		return;
	}

	z0 = eyex - centerx;
	z1 = eyey - centery;
	z2 = eyez - centerz;
	len = 1.0f / Math::Hypot(z0, z1, z2);
	z0 *= len;
	z1 *= len;
	z2 *= len;

	x0 = upy * z2 - upz * z1;
	x1 = upz * z0 - upx * z2;
	x2 = upx * z1 - upy * z0;
	len = Math::Hypot(x0, x1, x2);
	if (len == 0.0f) {
		x0 = 0.0f;
		x1 = 0.0f;
		x2 = 0.0f;
	}
	else {
		len = 1.0 / len;
		x0 *= len;
		x1 *= len;
		x2 *= len;
	}

	y0 = z1 * x2 - z2 * x1;
	y1 = z2 * x0 - z0 * x2;
	y2 = z0 * x1 - z1 * x0;
	len = Math::Hypot(y0, y1, y2);
	if (len == 0.0f) {
		y0 = 0.0f;
		y1 = 0.0f;
		y2 = 0.0f;
	}
	else {
		len = 1.0f / len;
		y0 *= len;
		y1 *= len;
		y2 *= len;
	}

	out->Values[0] = x0;
	out->Values[1] = y0;
	out->Values[2] = z0;
	out->Values[3] = 0.0f;
	out->Values[4] = x1;
	out->Values[5] = y1;
	out->Values[6] = z1;
	out->Values[7] = 0.0f;
	out->Values[8] = x2;
	out->Values[9] = y2;
	out->Values[10] = z2;
	out->Values[11] = 0.0f;
	out->Values[12] = -(x0 * eyex + x1 * eyey + x2 * eyez);
	out->Values[13] = -(y0 * eyex + y1 * eyey + y2 * eyez);
	out->Values[14] = -(z0 * eyex + z1 * eyey + z2 * eyez);
	out->Values[15] = 1.0f;
	return;
}

void Matrix4x4::Invert(Matrix4x4* out, Matrix4x4* in) {
	float* src = in->Values;
	float inv[16];

	inv[0] = src[5] * src[10] * src[15] - src[5] * src[11] * src[14] -
		src[9] * src[6] * src[15] + src[9] * src[7] * src[14] + src[13] * src[6] * src[11] -
		src[13] * src[7] * src[10];
	inv[1] = -src[1] * src[10] * src[15] + src[1] * src[11] * src[14] +
		src[9] * src[2] * src[15] - src[9] * src[3] * src[14] - src[13] * src[2] * src[11] +
		src[13] * src[3] * src[10];
	inv[2] = src[1] * src[6] * src[15] - src[1] * src[7] * src[14] - src[5] * src[2] * src[15] +
		src[5] * src[3] * src[14] + src[13] * src[2] * src[7] - src[13] * src[3] * src[6];
	inv[3] = -src[1] * src[6] * src[11] + src[1] * src[7] * src[10] +
		src[5] * src[2] * src[11] - src[5] * src[3] * src[10] - src[9] * src[2] * src[7] +
		src[9] * src[3] * src[6];
	inv[4] = -src[4] * src[10] * src[15] + src[4] * src[11] * src[14] +
		src[8] * src[6] * src[15] - src[8] * src[7] * src[14] - src[12] * src[6] * src[11] +
		src[12] * src[7] * src[10];
	inv[5] = src[0] * src[10] * src[15] - src[0] * src[11] * src[14] -
		src[8] * src[2] * src[15] + src[8] * src[3] * src[14] + src[12] * src[2] * src[11] -
		src[12] * src[3] * src[10];
	inv[6] = -src[0] * src[6] * src[15] + src[0] * src[7] * src[14] +
		src[4] * src[2] * src[15] - src[4] * src[3] * src[14] - src[12] * src[2] * src[7] +
		src[12] * src[3] * src[6];
	inv[7] = src[0] * src[6] * src[11] - src[0] * src[7] * src[10] - src[4] * src[2] * src[11] +
		src[4] * src[3] * src[10] + src[8] * src[2] * src[7] - src[8] * src[3] * src[6];
	inv[8] = src[4] * src[9] * src[15] - src[4] * src[11] * src[13] -
		src[8] * src[5] * src[15] + src[8] * src[7] * src[13] + src[12] * src[5] * src[11] -
		src[12] * src[7] * src[9];
	inv[9] = -src[0] * src[9] * src[15] + src[0] * src[11] * src[13] +
		src[8] * src[1] * src[15] - src[8] * src[3] * src[13] - src[12] * src[1] * src[11] +
		src[12] * src[3] * src[9];
	inv[10] = src[0] * src[5] * src[15] - src[0] * src[7] * src[13] -
		src[4] * src[1] * src[15] + src[4] * src[3] * src[13] + src[12] * src[1] * src[7] -
		src[12] * src[3] * src[5];
	inv[11] = -src[0] * src[5] * src[11] + src[0] * src[7] * src[9] +
		src[4] * src[1] * src[11] - src[4] * src[3] * src[9] - src[8] * src[1] * src[7] +
		src[8] * src[3] * src[5];
	inv[12] = -src[4] * src[9] * src[14] + src[4] * src[10] * src[13] +
		src[8] * src[5] * src[14] - src[8] * src[6] * src[13] - src[12] * src[5] * src[10] +
		src[12] * src[6] * src[9];
	inv[13] = src[0] * src[9] * src[14] - src[0] * src[10] * src[13] -
		src[8] * src[1] * src[14] + src[8] * src[2] * src[13] + src[12] * src[1] * src[10] -
		src[12] * src[2] * src[9];
	inv[14] = -src[0] * src[5] * src[14] + src[0] * src[6] * src[13] +
		src[4] * src[1] * src[14] - src[4] * src[2] * src[13] - src[12] * src[1] * src[6] +
		src[12] * src[2] * src[5];
	inv[15] = src[0] * src[5] * src[10] - src[0] * src[6] * src[9] - src[4] * src[1] * src[10] +
		src[4] * src[2] * src[9] + src[8] * src[1] * src[6] - src[8] * src[2] * src[5];

	float det = src[0] * inv[0] + src[1] * inv[4] + src[2] * inv[8] + src[3] * inv[12];
	if (det > 0.0f) {
		det = 1.0f / det;
		for (int i = 0; i < 16; i++) {
			out->Values[i] = inv[i] * det;
		}
	}
	else {
		for (int i = 0; i < 16; i++) {
			out->Values[i] = 0.0f;
		}
	}
}

void Matrix4x4::Print(Matrix4x4* out) {
	Log::Print(Log::LOG_INFO,
		"\n%6.2f %6.2f %6.2f %6.2f \n%6.2f %6.2f %6.2f %6.2f \n%6.2f %6.2f %6.2f %6.2f \n%6.2f %6.2f %6.2f %6.2f",
		out->Values[0],
		out->Values[1],
		out->Values[2],
		out->Values[3],
		out->Values[4],
		out->Values[5],
		out->Values[6],
		out->Values[7],
		out->Values[8],
		out->Values[9],
		out->Values[10],
		out->Values[11],
		out->Values[12],
		out->Values[13],
		out->Values[14],
		out->Values[15]);
}
