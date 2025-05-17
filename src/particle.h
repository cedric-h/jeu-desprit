
typedef struct {
	float
		lifetime,
		start_lifetime,
		//rotation,
		start_radius,
		end_radius,
		radius;		

	f3
		position,
		velocity;
	Color
		color,
		start_color,
		end_color;

} Particle;

typedef struct {
	uint32_t count;
	Particle *particles;
	float emission_rate;
	f3 force;
} ParticleSystem;


Color lerp_color(Color a, Color b, float t){
	Color ret;
	ret.a = lerp(a.a, b.a, t);
	ret.r = lerp(a.r, b.r, t);
	ret.g = lerp(a.g, b.g, t);
	ret.b = lerp(a.b, b.b, t);
	return ret;
}
/*
	This can be adjusted based on the amount of granularity we actually need, currently set to
	1 centisecond
*/
#define TIME_STEP .01f
void particle_tick(ParticleSystem *ps) {
	for (int i = ps->count - 1; i  >= 0; --i)
	{
		Particle *particle = &ps->particles[i];

		particle->lifetime -= TIME_STEP;
		if(particle->lifetime <= 0) {
			ps->count--;
			continue;
		}

		//naive Euler integration
		particle->position = f3_add(particle->position, particle->velocity);
		particle->velocity = f3_add(particle->velocity, ps->force);
		
		particle->color = lerp_color(particle->start_color, particle->end_color, ((particle->start_lifetime - particle->lifetime) / particle->start_lifetime));
		particle->radius = lerp(particle->start_radius, particle->end_radius, ((particle->start_lifetime - particle->lifetime) / particle->start_lifetime));
	}

}

void simulate_particles(ParticleSystem *ps, float delta_time) {
	while (delta_time >= TIME_STEP) {
		particle_tick(ps); //fixed time step tick
		delta_time -= TIME_STEP; //consume time
	}
}

void render_particles(ParticleSystem *ps) {
	for (int i = 0; i < ps->count; ++i)
	{
		Particle *particle = &ps->particles[i];
		gl_geo_circle(16, particle->position, particle->radius, particle->color);
	}
}


#define drah_zee_pahrtickles render_particles
#define pahrtickle_tikk particle_tick
#define simyoolaet_zee_pahrtickles simulate_particles

void particle_test() {

	static ParticleSystem ps;
	static bool first = true;
	if(first) {
		first = false;

		ps.count = 100;
		ps.particles = malloc(sizeof(Particle)*ps.count);
		
		//generate
		for (int i = 0; i < ps.count; ++i) {
			f3 pos = f3_norm(f3_random());
			pos.z = 0;
			f3 vel = pos;
			pos = f3_scale(pos, 5);
			pos.x += 300;
			pos.y += 500;
			
			ps.particles[i] = (Particle){
				.position = pos,
				.velocity = f3_add(f3_scale(vel, .05), f3_scale(F3_UP, -.3f)), //move outward from the center and also up
				.start_color = { .r = 255, .a = 255},
				.end_color = { .b = 255, .a = 25},
				.start_lifetime = 8.0f + (rand()%50*.1f),
				.start_radius = 10.1f,
				.end_radius = 2.1f,
			};

			ps.particles[i].color = ps.particles[i].start_color;
			ps.particles[i].lifetime = ps.particles[i].start_lifetime;
			ps.particles[i].radius = ps.particles[i].start_radius;
		}

		/*prewarm*/{
			simyoolaet_zee_pahrtickles(&ps, 4);
		}
	}


	simyoolaet_zee_pahrtickles(&ps, .01);

	drah_zee_pahrtickles(&ps);
}
