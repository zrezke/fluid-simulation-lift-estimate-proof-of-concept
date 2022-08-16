#pragma once

#include <SFML/Graphics.hpp>
#include "MeshImage.h"
#include <vector>
#include "VectorField.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>

#define AIR_DENSITY 1.29f

/**
 * @brief 
 * @param x x coordinate of effected squares.
 * @param upsideY pointer to array of two top y coordinates of effected squares.
 * @param downsideY pointer to array of two bottom y coordinates of effected squares.
 */
struct FoilEffectedSquares {
	int x;
	int* upsideY;
	int* downsideY;
};

class FluidSimulation : public sf::Drawable, public sf::Transformable
{
public:
	FluidSimulation(unsigned int size, float vectorFieldScale, float diffusion, float viscosity);

	void AddDensity(unsigned int x, unsigned int y, float amount);
	void AddVelocity(unsigned int x, unsigned int y, float amountX, float amountY);
	void Step(float dt, unsigned int iterations);
	void HandleMouse(sf::Window &window);
	void UpdateImage();
	void ExportFoil();
	void LoadFoil();

	bool vectorFieldActive = false;
	bool shouldClearWalls = false;
	bool shouldReset = false;
	std::vector<FoilEffectedSquares> foilEffectedSquares;
	

private:
	MeshImage meshImage;
	VectorField vectorField;
	float vectorFieldScale;

	unsigned int size;
	float diff, visc;
	std::vector<float> density0;
	std::vector<float> density;

	std::vector<float> vx;
	std::vector<float> vy;

	std::vector<float> vx0;
	std::vector<float> vy0;

	std::vector<int> iWall;
	std::vector<bool> bWall;

	bool *foil;

	virtual void draw(sf::RenderTarget &target, sf::RenderStates states) const
	{
		states.transform *= getTransform();
		target.draw(meshImage, states);
		if (vectorFieldActive)
			target.draw(vectorField, states);
	}

	enum Axis
	{
		none,
		xx,
		yy
	};

	void DetermineFoilEffectedSquares();
	void SetBounds(Axis axis, std::vector<float> &vec, unsigned int N);
	void LinearSolve(Axis axis, std::vector<float> &vec, std::vector<float> &vec0, float a, float c, unsigned int iterations, unsigned int N);
	void Diffuse(Axis axis, std::vector<float> &vec, std::vector<float> &vec0, float diff, float dt, unsigned int iterations, unsigned int N);
	void Project(std::vector<float> &vx, std::vector<float> &vy, std::vector<float> &p, std::vector<float> &div, unsigned int iterations, unsigned int N);
	void Advect(Axis axis, std::vector<float> &vec, std::vector<float> &vec0, std::vector<float> &vx, std::vector<float> &vy, float dt, unsigned int N);
};
