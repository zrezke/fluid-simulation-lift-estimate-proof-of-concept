#include "FluidSimulation.h"
#include <fstream>
#include <iostream>
//#define IX(i, j) (i + (N + 2) * j) // DOES NOT WORK, KEEP IN MIND IN FUTURE, This code is incorrect, saved for future reference to not repeat my mistake
#define IX(i, j) ((i) + (N + 2) * (j)) // Helper macro to convert a 2d index to a 1d index

FluidSimulation::FluidSimulation(unsigned int size, float vectorFieldScale, float diffusion, float viscosity)
{
	this->meshImage.Init(1, 1, size, size);																										// Initialize our MeshImage to 1x1 units since we use Scale on our fluidsimulation to decide the correct size.
	this->vectorField.Init(1, 1, int(size * vectorFieldScale), int(size * vectorFieldScale)); // Same goes for the vectorfield
	this->vectorFieldScale = vectorFieldScale;
	unsigned int N = size;
	// we add 2 units in each direction because we need surrounding cells in our simulation that
	// will act like boundaries and will need to have their values set to function properly, see SetBounds function.
	unsigned int NN = (N + 2) * (N + 2);

	this->size = size;
	this->diff = diffusion;
	this->visc = viscosity;

	this->density0.resize(NN, 0.f);
	this->density.resize(NN, 0.f);

	this->vx.resize(NN, 0.f);
	this->vy.resize(NN, 0.f);

	this->vx0.resize(NN, 0.f);
	this->vy0.resize(NN, 0.f);

	this->bWall.resize(NN, false);
}

void FluidSimulation::LoadFoil()
{
	std::cout << "Loading foil..." << std::endl;
	if (std::filesystem::exists("foil.bin"))
	{
		std::ifstream file;
		file.open("foil.bin", std::ios::in);
		unsigned int NN = (this->size + 2) * (this->size + 2);
		std::cout << NN << std::endl;
		this->foil = new bool[NN];
		std::string line = "";
		getline(file, line);
		std::string integer = "";
		for (unsigned int i = 0; i < line.length(); i++)
		{
			if (line[i] == ' ')
			{
				this->foil[std::stoi(integer)] = true;
				integer = "";
			}
			else
			{
				integer += line[i];
			}
		}
		file.close();
		iWall.clear();
		for (int i = 0; i < NN; i++)
		{
			if (this->foil[i])
			{
				this->bWall[i] = true;
				this->iWall.push_back(i);
			}
		}
	}
	this->DetermineFoilEffectedSquares();
}

/**
 * @brief If nothing is above the point, find find the most below point and take two squares on each side as the effected squares
 *
 */
void FluidSimulation::DetermineFoilEffectedSquares()
{
	unsigned int N = this->size * 2;
	unsigned int NN = N * N;
/* 	for (int i = 0; i < NN; i++)
	{
		std::cout << this->foil[i] << " ";
		if (i % N == 0)
		{
			std::cout << std::endl;
		}
	} */
	this->foilEffectedSquares.clear();
	for (int i = 0; i < NN; i++)
	{
		if (this->foil[i])
		{
			int y = i / N;
			int x = i % N;
			bool isTop = true;
			for (int j = y; j >= 0; j--)
			{
				if (this->bWall[x + j * N])
				{
					isTop = false;
					break;
				}
			}
			if (isTop)
			{
				//std::cout << isTop << std::endl;
				int mostBelow = y;
				for (int j = y; j < N; j++)
				{
					if (this->foil[x + j * N])
					{
						mostBelow = j;
					}
				}
				FoilEffectedSquares effectedSquares;
				effectedSquares.x = x;
				effectedSquares.upsideY = new int[2]{y, y - 1};
				effectedSquares.downsideY = new int[2]{mostBelow, mostBelow + 1};
				this->foilEffectedSquares.push_back(effectedSquares);
			}
		}
	}
}

void FluidSimulation::ExportFoil()
{
	std::ofstream data_file;
	data_file.open("foil.bin", std::ofstream::out);
	unsigned int NN = (size + 2) * (size + 2);

	bool *array = new bool[NN];
	for (const int &i : iWall)
	{
		data_file << i << ' ';
		array[i] = true;
	}
	data_file.close();
}

void FluidSimulation::HandleMouse(sf::Window &window)
{

	// Checks if walls should be cleared, should probably be in another function than
	// HandleMouse but didnt feel like adding a separate function call just for this and resetting simulation.
	if (iWall.size() > 0)
	{
		if (shouldClearWalls)
		{
			std::fill(bWall.begin(), bWall.end(), false);
			iWall.clear();
		}
	}
	else if (!shouldClearWalls)
		shouldClearWalls = true;

	// Checks if simulation should be reset, should probably be in another function than
	// HandleMouse but didnt feel like adding a separate function call just for this and clearing walls.
	if (shouldReset)
	{
		std::fill(density0.begin(), density0.end(), 0.f);
		std::fill(density.begin(), density.end(), 0.f);

		std::fill(vx.begin(), vx.end(), 0.f);
		std::fill(vy.begin(), vy.end(), 0.f);

		std::fill(vx0.begin(), vx0.end(), 0.f);
		std::fill(vy0.begin(), vy0.end(), 0.f);
		shouldReset = false;
	}

	int N = this->size; // We need to define N to be able to use IX macro later in the function.

	// Calculate where the mouse is in the simulation
	static sf::Vector2f lastMousePos;
	auto mouse = sf::Mouse::getPosition(window);
	sf::Vector2f position = getPosition();
	sf::Vector2f scale = getScale();
	sf::Vector2f mousePos((mouse.x - position.x) / scale.x, (mouse.y - position.y) / scale.y);
	sf::Vector2u quadCoord = meshImage.fromScreenSpace(mousePos);
	quadCoord.x++; // Correct our mouse position since we have the 2 extra cells in each axis and we never want to touch the boundaries.
	quadCoord.y++;
	if (quadCoord.x < 1 || quadCoord.x > size || quadCoord.y < 1 || quadCoord.y > size)
		return; // Abort if mouse is outside of simulation

	if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) // Check if left mouse is down
			&& !bWall[IX(quadCoord.x, quadCoord.y)])						// make sure the current cell isn't a wall
	{
		for (int i = -19; i < 20; i++)
		{
			AddDensity(quadCoord.x, quadCoord.y + i, 0.25f);										// Add some density to sim
			AddVelocity(quadCoord.x, quadCoord.y + i, 1 * this->size / 200, 0); // Add some velocity in the direciton we moved the mouse in the sim.
		}
	}

	// Add walls in a 2x2 pattern at mouse position, mouse is on top left cell that is placed.
	if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right) && !(quadCoord.x > size - 1 || quadCoord.y > size - 1))
	{
		if (!bWall[IX(quadCoord.x, quadCoord.y)])
		{
			bWall[IX(quadCoord.x, quadCoord.y)] = true;
			iWall.push_back(IX(quadCoord.x, quadCoord.y));
		}
		if (!bWall[IX(quadCoord.x + 1, quadCoord.y)])
		{
			bWall[IX(quadCoord.x + 1, quadCoord.y)] = true;
			iWall.push_back(IX(quadCoord.x + 1, quadCoord.y));
		}
		if (!bWall[IX(quadCoord.x, quadCoord.y + 1)])
		{
			bWall[IX(quadCoord.x, quadCoord.y + 1)] = true;
			iWall.push_back(IX(quadCoord.x, quadCoord.y + 1));
		}
		if (!bWall[IX(quadCoord.x + 1, quadCoord.y + 1)])
		{
			bWall[IX(quadCoord.x + 1, quadCoord.y + 1)] = true;
			iWall.push_back(IX(quadCoord.x + 1, quadCoord.y + 1));
		}
		shouldClearWalls = false; // resets the clear wall bool so walls can now be cleared with UI button.
	}
	lastMousePos = mousePos;
}

// This takes care of updating our density visualization on the MeshImage and the velocities on the VectorField
void FluidSimulation::UpdateImage()
{
	int N = this->size;
	// Go through all cells except the boundary cells since we don't want to display those.
	for (int x = 1, x0 = -1; x <= N; x++)
		for (int y = 1, y0 = -1; y <= N; y++)
		{
			sf::Vector2u coord(x - 1, y - 1);
			if (!bWall[IX(x, y)]) // Check that current cell is not a wall
			{
				float d = density[IX(x, y)] * 10;
				if (d < 0)
					d = 0;

				int r = 255 * d;
				int g = 10 * d;
				int b = 20 * d * d;
				if (r > 255)
					r = 255;
				if (g > 255)
					g = 255;
				if (b > 255)
					b = 255;
				meshImage.setColor(coord, sf::Color(r, g, b)); // Set color at current position corresponding to cell density value.
			}
			else
				meshImage.setColor(coord, sf::Color::Green); // If current cell is a wall just make it green.

			if (vectorFieldActive)
			{ // If we are displaying the VectorField, update the vector arrows.
				// Make sure we go past a certain amount of cells relative to the scale of the VectorField before updating new Vector Arrow
				if (int(x * vectorFieldScale - vectorFieldScale / 2) > x0 || int(y * vectorFieldScale - vectorFieldScale / 2) > y0)
				{
					x0 = int(x * vectorFieldScale - vectorFieldScale / 2); // Find corresponding index of arrow in field
					y0 = int(y * vectorFieldScale - vectorFieldScale / 2);
					sf::Vector2u vecCoord(x0, y0);
					if (!bWall[IX(x, y)])
						vectorField.setVector(vecCoord, sf::Vector2f(vx[IX(x, y)], vy[IX(x, y)])); // Set vector arrow to the velocity values

					else																									 // If we are on a wall cell, just make it (0,0). (The static objects are not perfect and velocity does travel
						vectorField.setVector(vecCoord, sf::Vector2f(0, 0)); // through them, but not much, so just looks like noise in the objects otherwise.
				}
			}
		}
}

void FluidSimulation::AddDensity(unsigned int x, unsigned int y, float amount)
{
	int N = this->size;
	this->density[IX(x, y)] += amount;
}

void FluidSimulation::AddVelocity(unsigned int x, unsigned int y, float amountX, float amountY)
{
	int N = this->size;
	int index = IX(x, y);
	this->vx[index] += amountX;
	this->vy[index] += amountY;
}

// The heart of the simulation, this function will step through time in the simulation, a higher dt value is a larger step in time, a larger iteration value is a more precise but expensive simulation step.
void FluidSimulation::Step(float dt, unsigned int iterations)
{
	Diffuse(Axis::xx, vx0, vx, visc, dt, iterations, this->size); // Diffuse the velocities on the x axis depending on the viscosity of the fluid
	Diffuse(Axis::yy, vy0, vy, visc, dt, iterations, this->size); // Same, but on y axis

	Project(vx0, vy0, vx, vy, iterations, this->size); // Since simulation isn't perfect we need to fix the divergence so that it stays at 0, i.e. make total values not change over the simulation.

	Advect(Axis::xx, vx, vx0, vx0, vy0, dt, this->size); // Advect/Move the velocity values towards where they would move some mass, but make them act on themselves.
	Advect(Axis::yy, vy, vy0, vx0, vy0, dt, this->size);

	Project(vx, vy, vx0, vy0, iterations, this->size); // Once again, fix divergence

	Diffuse(Axis::none, density0, density, diff, dt, iterations, this->size); // Diffuse our density so that it can spread out according to our diffusion value
	Advect(Axis::none, density, density0, vx, vy, dt, this->size);						// Advect/move our density values according to the velocities.

	float lift = 0.00f;
	unsigned int N = this->size;
	for (FoilEffectedSquares &square : this->foilEffectedSquares)
	{
		float v2 = (this->vx[IX(square.x, square.upsideY[0])] + this->vy[IX(square.x, square.upsideY[1])]) / 2;
		float v1 = (this->vx[IX(square.x, square.downsideY[0])] + this->vy[IX(square.x, square.downsideY[1])]) / 2;
		lift += 0.5f * AIR_DENSITY * (v2 * v2 - v1 * v1);
	}
	std::cout << lift << std::endl;
}

// This function is responsible for the fluid to simulate walls around the simulation and to make them interact with our walls/static objects in the sim.
void FluidSimulation::SetBounds(Axis axis, std::vector<float> &vec, unsigned int N)
{
	// Go around the borders of the simulation and make them counteract the velocities in their respective axes (if called with velocity values)
	// if called with density values, simply copy the neighbouring values to not make the boundaries "suck up the densities into the void".
	for (unsigned int i = 1; i <= N; i++)
	{
		vec[IX(0, i)] = axis == Axis::xx ? -vec[IX(1, i)] : vec[IX(1, i)];
		vec[IX(N + 1, i)] = vec[IX(N, i)];
		vec[IX(i, 0)] = axis == Axis::yy ? -vec[IX(i, 1)] : vec[IX(i, 1)];
		vec[IX(i, N + 1)] = axis == Axis::yy ? -vec[IX(i, N)] : vec[IX(i, N)];
	}
	// Special case for the corners, make them take the average values of their neighbouring boundary cells.
	vec[IX(0, 0)] = 0.5f * (vec[IX(1, 0)] + vec[IX(0, 1)]);
	vec[IX(0, N + 1)] = 0.5f * (vec[IX(1, N + 1)] + vec[IX(0, N)]);
	vec[IX(N + 1, 0)] = 0.5f * (vec[IX(N, 0)] + vec[IX(N + 1, 1)]);
	vec[IX(N + 1, N + 1)] = 0.5f * (vec[IX(N, N + 1)] + vec[IX(N + 1, N)]);

	// This will pretty much do the same as above but instead work for the static objects, aka walls.
	// This works since we know we will always have at least a group of 2x2 wall cells.
	for (int i = 0; i < iWall.size(); i++)
	{
		int index = iWall[i];
		switch (axis)
		{
		case Axis::xx: // Check which side the wall is on and counteract it's neighbours velocity (not wall neighbour)
			if (!bWall[index + IX(1, 0)])
				vec[index] = -vec[index + IX(1, 0)];
			else
				vec[index] = -vec[index + IX(-1, 0)];
			break;
		case Axis::yy: // same as for x, but this time for y axis
			if (!bWall[index + IX(0, 1)])
				vec[index] = -vec[index + IX(0, 1)];
			else
				vec[index] = -vec[index + IX(0, -1)];
			break;
		case Axis::none: // We will work here if we are setting bounds for the densities.
										 // here, we will simply take the average values of the surrounding
										 // cells that are not walls and use that as our density value for the current wall cell.
										 // This method is not perfect and will usually get some net loss in the simulation but works
										 // well enough for our purposes.
			vec[index] = 0;
			int count = 0;

			// Get values from neighbouring cells that are horizontal and vertical to our current cell
			if (!bWall[index + IX(1, 0)])
			{
				vec[index] += vec[index + IX(1, 0)];
				count++;
			}
			if (!bWall[index + IX(-1, 0)])
			{
				vec[index] += vec[index + IX(-1, 0)];
				count++;
			}
			if (!bWall[index + IX(0, 1)])
			{
				vec[index] += vec[index + IX(0, 1)];
				count++;
			}
			if (!bWall[index + IX(0, -1)])
			{
				vec[index] += vec[index + IX(0, -1)];
				count++;
			}

			// Get values from neighbouring cells that are diagonal to our current cell
			if (!bWall[index + IX(1, 1)])
			{
				vec[index] += vec[index + IX(1, 1)];
				count++;
			}
			if (!bWall[index + IX(-1, 1)])
			{
				vec[index] += vec[index + IX(-1, 1)];
				count++;
			}
			if (!bWall[index + IX(1, -1)])
			{
				vec[index] += vec[index + IX(1, -1)];
				count++;
			}
			if (!bWall[index + IX(-1, -1)])
			{
				vec[index] += vec[index + IX(-1, -1)];
				count++;
			}

			if (count > 0)
				vec[index] /= count; // divide our total value with the number of non wall
														 // cells we found and copied from, if we had any neighbours that were not walls.
		}
	}
}

// This is an implementation of the Gauss-Seidel method to approximate system of equations. However I don't know the exact math behind it so
// credit goes to Jos Stam for how it works, all I did was extract it from his density function and rewrite it slightly for it to work with my code.
void FluidSimulation::LinearSolve(Axis axis, std::vector<float> &vec, std::vector<float> &vec0, float a, float c, unsigned int iterations, unsigned int N)
{
	for (unsigned int k = 0; k < iterations; k++)
	{
		for (unsigned int i = 1; i <= N; i++)
			for (unsigned int j = 1; j <= N; j++)
			{
				vec[IX(i, j)] = (vec0[IX(i, j)] + a * (vec[IX(i - 1, j)] + vec[IX(i + 1, j)] +
																							 vec[IX(i, j - 1)] + vec[IX(i, j + 1)])) /
												c;
			}
		SetBounds(axis, vec, N);
	}
}

// The code for diffusing values, again I don't know the exact math behind the LinearSolve function so I took inspiration from Mike Ash's 3D Fluid Simulation but changed it for 2D.
void FluidSimulation::Diffuse(Axis axis, std::vector<float> &vec, std::vector<float> &vec0, float diff, float dt, unsigned int iterations, unsigned int N)
{
	float a = dt * diff * N * N;															 // I am assuming this is the diffusion rate combined with how big our grid is, but this is mostly an educated guess.
	LinearSolve(axis, vec, vec0, a, 1 + 4 * a, iterations, N); // This will solve for the diffusion, the result is that the data in all cells in the simulation will start to converge towards being equally distributed.
}

// The projection function will approximate our velocities to keep divergence close to 0. (It should be 0, but we are only approximating towards it).
// It does this by calculating (approximating) the divergence and removing it from the field, which gives us a divergence free field (divergence = 0).
void FluidSimulation::Project(std::vector<float> &vx, std::vector<float> &vy, std::vector<float> &p, std::vector<float> &div, unsigned int iterations, unsigned int N)
{
	float h;
	h = 1.0f / N;
	for (unsigned int i = 1; i <= N; i++)
	{
		for (unsigned int j = 1; j <= N; j++)
		{
			div[IX(i, j)] = -0.5f * h * (vx[IX(i + 1, j)] - vx[IX(i - 1, j)] + vy[IX(i, j + 1)] - vy[IX(i, j - 1)]);
			p[IX(i, j)] = 0;
		}
	}
	SetBounds(Axis::none, div, N);
	SetBounds(Axis::none, p, N);
	LinearSolve(Axis::none, p, div, 1, 4, iterations, N);
	for (unsigned int i = 1; i <= N; i++)
	{
		for (unsigned int j = 1; j <= N; j++)
		{
			vx[IX(i, j)] -= 0.5f * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) / h;
			vy[IX(i, j)] -= 0.5f * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) / h;
		}
	}
	SetBounds(Axis::xx, vx, N);
	SetBounds(Axis::yy, vy, N);
}

// This function will move our values we use it with, with the velocity in the field.
// We do this by checking where our values would go if we went back in time, and then simply taking the values from this
// position we find and appying it to the current position we are updating.
//
// Since our position will not be (most likely) exactly right in the middle of a cell we always check in a grid of 2x2 values
// to copy from. We do take into account how much of each grid we will get values from with a linear interpolation.
void FluidSimulation::Advect(Axis axis, std::vector<float> &vec, std::vector<float> &vec0, std::vector<float> &vx, std::vector<float> &vy, float dt, unsigned int N)
{
	int i0, j0, i1, j1;
	float x, y, frac_x_inv, frac_y_inv, frac_x, frac_y, dt0;
	dt0 = dt * N;
	for (unsigned int i = 1; i <= N; i++)
	{
		for (unsigned int j = 1; j <= N; j++)
		{
			x = i - dt0 * vx[IX(i, j)]; // Find the position if we went back in time.
			y = j - dt0 * vy[IX(i, j)];

			// Make sure we stay within the grid, since we can't get values to move in from the outside.
			if (x < 0.5f)
				x = 0.5f;
			if (x > N + 0.5f)
				x = N + 0.5f;
			i0 = (int)x; // Get the leftmost x coordinate in our 2x2 grid
			i1 = i0 + 1; // Get the rightmost x

			if (y < 0.5f)
				y = 0.5f;
			if (y > N + 0.5f)
				y = N + 0.5f;
			j0 = (int)y; // Get the upper y coordinate in our 2x2 grid
			j1 = j0 + 1; // get the lower y (larger y => lower down)

			frac_x = x - i0;				 // extract decimal value from x so we know how much we will extract from the left
			frac_x_inv = 1 - frac_x; // same but for the right side

			frac_y = y - j0;				 // extract decimal value from y so we know how much we will extract from the top
			frac_y_inv = 1 - frac_y; // same but for the bottom

			vec[IX(i, j)] = // Linearly approximate our new value from the 2x2 grid and fractions we found earlier.
					frac_x_inv * (frac_y_inv * vec0[IX(i0, j0)] +
												frac_y * vec0[IX(i0, j1)]) +
					frac_x * (frac_y_inv * vec0[IX(i1, j0)] +
										frac_y * vec0[IX(i1, j1)]);
		}
	}
	SetBounds(axis, vec, N);
}
