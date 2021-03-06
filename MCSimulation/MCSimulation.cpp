#include "stdafx.h"
#include <iostream>
#include <stdlib.h>
#include <random> 
#include <fstream>

#define PI 3.14159265359

std::random_device rng;
std::mt19937 gen(rng());
std::uniform_real_distribution<float> distr(0.0f, 1.0f);

// Henyey-Greenstein scattering phase function
// https://www.astro.umd.edu/~jph/HG_note.pdf
float GetCosTheta(float g) {
	if (g == 0) return 2.0f * distr(rng) - 1.0f;

	float mu = (1.0f - g * g) / (1.0f - g + 2.0f * g * distr(rng));
	return (1.0f * g * g - mu * mu) / (2.0f * g);
}

void ScatterPhoton(float &mu_x, float &mu_y, float &mu_z, float g) {
	// Theta angle
	float cosTheta = GetCosTheta(g);
	float phi = 2.0f * PI * distr(rng);

	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	float sinPhi = std::sin(phi);
	float cosPhi = std::cos(phi);

	// Rotate using Rodrigues' rotation formula
	// https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
	if (mu_z == 1.0) {
		mu_x = sinTheta * cosPhi;
		mu_y = sinTheta * sinPhi;
		mu_z = cosTheta;
	}
	else if (mu_z == -1.0) {
		mu_x = sinTheta * cosPhi;
		mu_y = -sinTheta * sinPhi;
		mu_z = -cosTheta;
	}
	else {
		float denom = sqrt(1.0 - mu_z * mu_z);
		float muzcosphi = mu_z * cosPhi;
		float ux = sinTheta * (mu_x * muzcosphi - mu_y * sinPhi) / denom + mu_x * cosTheta;
		float uy = sinTheta * (mu_y * muzcosphi + mu_x * sinPhi) / denom + mu_y * cosTheta;
		float uz = -denom * sinTheta * cosPhi + mu_z * cosTheta;
		mu_x = ux, mu_y = uy, mu_z = uz;
	}
}

void MCSimulation(float *&records, const uint32_t &size) {
	const uint32_t nrPhotons = 1000000; // Amount of photons used for the simulation
	float scale = 1.0f / (float)nrPhotons;
	float sigma_a = 1.0f, sigma_s = 2.0f, sigma_t = sigma_a + sigma_s;
	float d = 0.5f;  // The thickness of a slab
	float g = 0.75f; // The variable used for scattering (1 = dominant scattering towards the direction of the photon, -1 the opposite)
	float slabSize = 2.0f;
	static const short m = 10; // The value used for the Russian Roulette technique

	float Rd = 0.0f; // The amount of photons that exit the slab from the side it entered from
	float Tt = 0.0f; // The amount of photons that exit from the other side of the slab than where they entered

	for (size_t i = 0; i < nrPhotons; i++) {
		float weight = 1.0f;

		// The initial position and direction of the photons
		float x = 0.0f, y = 0.0f, z = 0.0f, mu_x = 0.0f, mu_y = 0.0f, mu_z = 1.0f;

		// Keep simulating the photon until it is either absorbed or until it exits the slab
		while (weight != 0.0f) {
			// The random value picked with the PDF of the Beer-Lambert equation using the inverse sampling method
			float s = -log(distr(rng)) / sigma_t;
			float dist = 0.0f;

			// Get the distance from the photon to the edge of the slab
			if (mu_z > 0.0f) dist = (d - z) / mu_z;
			else if (mu_z < 0.0f) dist = -z / mu_z; // Here muz is negative so use -z

			// Check if the photon exited the slab
			if (s > dist) {
				if (mu_z > 0.0f) Tt += weight;
				else Rd += weight;

				int xi = (int)((x + slabSize / 2.0f) / slabSize * size);
				int yi = (int)((y + slabSize / 2.0f) / slabSize * size);
				if (mu_z > 0.0f && xi >= 0 && x < size && yi >= 0 && yi < size) {
					records[yi * size + xi] += weight;
				}

				break;
			}

			// Update position
			x += s * mu_x;
			y += s * mu_y;
			z += s * mu_z;

			// Absorb the photon
			float dw = sigma_a / sigma_t;
			weight -= dw;
			weight = std::max(0.0f, weight);

			if (weight < 0.001f) {
				// If the photon got absorbed, finish the simulation
				if (distr(rng) > 1.0f / m) break;
				else weight *= m;
			}

			// Apply scattering
			ScatterPhoton(mu_x, mu_y, mu_z, g);
		}
	}

	printf("Rd %f Tt %f\n", Rd * scale, Tt * scale);
}

int main()
{
	uint32_t size = 512; // Size of the output image
	uint32_t nrPasses = 64; // Number of passes to do (more = more accurate simulation)
	float color[3] = { 0.0f, 0.77f, 0.80f }; // The color of the photons

	float *records = new float[size * size * 3];
	float *pixels = new float[size * size];

	for (uint32_t i = 1; i < nrPasses; i++) {
		MCSimulation(records, size);
		for (int j = 0; j < size * size; j++) {
			pixels[j] = records[j] / i;
		}
	}

	// Save the result into a file
	std::ofstream ofs("out.png", std::ios::out | std::ios::binary);
	ofs << "P6\n" << size << " " << size << "\n255\n";
	for (uint32_t i = 0; i < size * size; i++) {
		unsigned char rval = (unsigned char)(255 * color[0] * std::min(1.0f, pixels[i]));
		unsigned char gval = (unsigned char)(255 * color[1] * std::min(1.0f, pixels[i]));
		unsigned char bval = (unsigned char)(255 * color[2] * std::min(1.0f, pixels[i]));

		ofs << rval << gval << bval;
	}

	ofs.close();

	delete[] records;
	delete[] pixels;

	std::cout << "Simulation done.\n";

	getchar();

	return 0;
}