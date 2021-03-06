/*! \file   SparseBackPropagation.h
	\author Gregory Diamos
	\date   Sunday December 22, 2013
	\brief  The header file for the SparseBackPropagation class.
*/

#pragma once

#include <minerva/neuralnetwork/interface/BackPropagation.h>

namespace minerva
{

namespace neuralnetwork
{

/*! \brief Implements back propagation using a cost function with a sparsity constraint */
class SparseBackPropagation : public BackPropagation
{
public:
	SparseBackPropagation(NeuralNetwork* ann = nullptr,
		BlockSparseMatrix* input = nullptr,
		BlockSparseMatrix* ref = nullptr);

public:
	virtual BlockSparseMatrixVector getCostDerivative(const NeuralNetwork&, const BlockSparseMatrix&, const BlockSparseMatrix& ) const;
	virtual BlockSparseMatrix getInputDerivative(const NeuralNetwork&, const BlockSparseMatrix&, const BlockSparseMatrix&) const;
	virtual float getCost(const NeuralNetwork&, const BlockSparseMatrix&, const BlockSparseMatrix&) const;
	virtual float getInputCost(const NeuralNetwork&, const BlockSparseMatrix&, const BlockSparseMatrix&) const;

private:
	// Used for controlling the magnitude of neuron weights
	float _lambda;
	// The ideal probability for a neuron to fire (i.e. 0.05 -> it should fire 5% of the time on average)
	float _sparsity;
	// Used for controlling the sparsity of neuron responses (it penalizes all of the neurons firing at once)
	float _sparsityWeight;

};

}

}



