#define F3_UP      ((f3){ 0, 1, 0 })
#define F3_RIGHT   ((f3){ 1, 0, 0 })
#define F3_FORWARD ((f3){ 0, 0, 1 })
#define F3_DOWN    ((f3){ 0, -1, 0 })
#define F3_LEFT    ((f3){ -1, 0, 0 })
#define F3_BACK    ((f3){ 0, 0, -1 })
f3 f3_add(f3 a, f3 b) { return (f3) { a.x + b.x, a.y + b.y, a.z + b.z }; }
f3 f3_sub(f3 a, f3 b) { return (f3) { a.x - b.x, a.y - b.y, a.z - b.z }; }
f3 f3_mul(f3 a, f3 b) { return (f3) { a.x * b.x, a.y * b.y, a.z * b.z }; }
f3 f3_div(f3 a, f3 b) { return (f3) { a.x / b.x, a.y / b.y, a.z / b.z }; }

f3 f3_scale(f3 v, float s) { return (f3) { v.x * s, v.y * s, v.z * s }; }

f3 f3_random() {
	return (f3){ .x = rand()%100-50, .y = rand()%100-50, .z = rand()%100-50 };
}
