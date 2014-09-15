/* Author: Sudnya Padalikar
 * Date  : 08/17/2013
 * The implementation of the dense back propagate data class
 */

#include <minerva/neuralnetwork/interface/DenseBackPropagation.h>
#include <minerva/neuralnetwork/interface/NeuralNetwork.h>

#include <minerva/matrix/interface/Matrix.h>
#include <minerva/matrix/interface/BlockSparseMatrix.h>
#include <minerva/matrix/interface/BlockSparseMatrixVector.h>

// Minerva Includes
#include <minerva/util/interface/debug.h>
#include <minerva/util/interface/Knobs.h>

// Standard Library Includes
#include <algorithm>
#include <cmath>

namespace minerva
{

namespace neuralnetwork
{

typedef matrix::Matrix Matrix;
typedef matrix::BlockSparseMatrix BlockSparseMatrix;
typedef Matrix::FloatVector FloatVector;
typedef DenseBackPropagation::BlockSparseMatrixVector BlockSparseMatrixVector;

static bool isInMargin(const Matrix& ref, const Matrix& output, float epsilon)
{
	return output.subtract(ref).abs().reduceSum() < (ref.size() * epsilon);
}

static float computeCostForLayer(const Layer& layer, const BlockSparseMatrix& layerInput,
	const BlockSparseMatrix& layerOutput, float lambda)
{

	#if 0
	//J(theta) = -1/m (sum over i, sum over k yi,k * log (h(xl)k) + (1-yik)*log(1-h(xi)k) +
	//		   regularization term lambda/2m sum over l,i,j (theta[i,j])^2
	
	auto hx = layer.runInputs(layerInput);
	
	auto logHx = hx.log();
	
	auto oneMinusHx = hx.negate().add(1.0f);
	
	// add an epsilon to avoid log(0)
	auto logOneMinusHx = oneMinusHx.add(1e-15f).log();
	
	auto oneMinusY = layerOutput.negate().add(1.0f);

	unsigned m = layerInput.rows();

	auto yTemp = layerOutput.elementMultiply(logHx);

	auto yMinusOneTemp = oneMinusY.elementMultiply(logOneMinusHx);
	
	auto sum = yTemp.add(yMinusOneTemp);
	auto cost = sum.multiply(-1.0f/m);

	float costSum = cost.reduceSum();
	#else
	unsigned m = layerInput.rows();

	auto hx = layer.runInputs(layerInput);

	auto errors = hx.subtract(layerOutput);
	auto squaredErrors = errors.elementMultiply(errors);

	float sumOfSquaredErrors = squaredErrors.reduceSum();
	
	float costSum = sumOfSquaredErrors * 1.0f / (2.0f * m);

	#endif

	costSum += (lambda / (2.0f)) * (layer.getWeightsWithoutBias().elementMultiply(
		layer.getWeightsWithoutBias()).reduceSum());

	return costSum;
}

static float computeCostForNetwork(const NeuralNetwork& network, const BlockSparseMatrix& input,
	const BlockSparseMatrix& referenceOutput, float lambda)
{
	//J(theta) = -1/m (sum over i, sum over k y(i,k) * log (h(x)) + (1-y(i,k))*log(1-h(x)) +
	//		   regularization term lambda/2m sum over l,i,j (theta[i,j])^2
	// J = (1/m) .* sum(sum(-yOneHot .* log(hx) - (1 - yOneHot) .* log(1 - hx)));
	#if 0
	const float epsilon = 1.0e-40f;

	unsigned m = input.rows();

	auto hx = network.runInputs(input);
	
	auto logHx = hx.add(epsilon).log();
	auto yTemp = referenceOutput.elementMultiply(logHx);

	auto oneMinusY = referenceOutput.negate().add(1.0f);
	auto oneMinusHx = hx.negate().add(1.0f);
	auto logOneMinusHx = oneMinusHx.add(epsilon).log(); // add an epsilon to avoid log(0)
	auto yMinusOneTemp = oneMinusY.elementMultiply(logOneMinusHx);

	auto sum = yTemp.add(yMinusOneTemp);

	float costSum = sum.reduceSum() * -0.5f / m;
	#else

	unsigned m = input.rows();

	auto hx = network.runInputs(input);

	auto errors = hx.subtract(referenceOutput);
	auto squaredErrors = errors.elementMultiply(errors);

	float sumOfSquaredErrors = squaredErrors.reduceSum();
	
	float costSum = sumOfSquaredErrors * 1.0f / (2.0f * m);

	#endif

	if(lambda > 0.0f)
	{
		for(auto& layer : network)
		{
			costSum += (lambda / (2.0f)) * ((layer.getWeightsWithoutBias().elementMultiply(
				layer.getWeightsWithoutBias())).reduceSum());
		}
	}
	
	return costSum;
}

static bool gradientChecking(const BlockSparseMatrix& partialDerivatives,
	const Layer& layer, const BlockSparseMatrix& layerInput,
	const BlockSparseMatrix& layerOutput, float epsilon, float lambda)
{
	auto layerWeights = layer.getFlattenedWeights();
	auto flattenedPartialDerivatives = BackPropagation::flatten(partialDerivatives);

	Matrix gradientEstimate(flattenedPartialDerivatives.rows(), flattenedPartialDerivatives.columns());

	util::log("DenseBackPropagation") << "Running gradient checking on " << layerWeights.size() << " weights....\n";

	assertM(layerWeights.rows() == flattenedPartialDerivatives.rows(), "Layer weights has " << layerWeights.rows()
		<< " rows, but the flattened partial derivatives has " << flattenedPartialDerivatives.rows());
	assert(layerWeights.columns() == flattenedPartialDerivatives.columns());

	auto weight = layerWeights.begin();
	auto partialDerivative = flattenedPartialDerivatives.begin();
	for (auto estimate = gradientEstimate.begin(); estimate != gradientEstimate.end(); ++estimate, ++weight, ++partialDerivative)
	{
		// +e
		Layer layerPlus = layer;

		layerPlus.back()[std::distance(layerWeights.begin(), weight)] += epsilon;

		util::log("DenseBackPropagation") << "  layer plus e " << layerPlus.back().toString() << "\n";
		// -e 
		Layer layerMinus = layer;

		layerMinus.back()[std::distance(layerWeights.begin(), weight)] -= epsilon;
		util::log("DenseBackPropagation") << "  layer minus e " << layerMinus.back().toString() << "\n";

		// update derivative value
		float derivative = (computeCostForLayer(layerPlus, layerInput, layerOutput, lambda) -
			computeCostForLayer(layerMinus, layerInput, layerOutput, lambda)) / (2.0f * epsilon);
	
		*estimate = derivative;

		util::log("DenseBackPropagation") << " gradient of weight "
			<< std::distance(layerWeights.begin(), weight) << " out of " << layerWeights.size() 
			<< " weights is " << derivative << ", compared to computed " << *partialDerivative << "\n";
	
		 if (std::abs(derivative - *partialDerivative) > epsilon)
		 {
			return false;
		 }
	}

	return isInMargin(flattenedPartialDerivatives, gradientEstimate, epsilon);
}

DenseBackPropagation::DenseBackPropagation(NeuralNetwork* ann, BlockSparseMatrix* input, BlockSparseMatrix* ref)
 : BackPropagation(ann, input, ref), _lambda(0.0f)
{
	_lambda = util::KnobDatabase::getKnobValue("NeuralNetwork::Lambda", 0.01f);
}

BlockSparseMatrixVector DenseBackPropagation::getCostDerivative(const NeuralNetwork& network,
	const BlockSparseMatrix& input,
	const BlockSparseMatrix& reference) const
{
	return getCostDerivative(network);
}

BlockSparseMatrix DenseBackPropagation::getInputDerivative(const NeuralNetwork& network,
	const BlockSparseMatrix& input, const BlockSparseMatrix&) const
{
	return getInputDerivative(network, input);
}

float DenseBackPropagation::getCost(const NeuralNetwork& network, const BlockSparseMatrix& input,
	const BlockSparseMatrix& reference) const
{
	return computeCostForNetwork(network, input, reference, _lambda);
}

float DenseBackPropagation::getInputCost(const NeuralNetwork& network, const BlockSparseMatrix& input,
	const BlockSparseMatrix& reference) const
{
	return computeCostForNetwork(network, input, reference, 0.0f);
}

BlockSparseMatrixVector DenseBackPropagation::getDeltas(const NeuralNetwork& network, const BlockSparseMatrixVector& activations) const
{
	BlockSparseMatrixVector deltas;

	deltas.reserve(activations.size() - 1);
	
	auto i = activations.rbegin();
	auto delta = (*i).subtract(*_referenceOutput).elementMultiply(i->sigmoidDerivative());
	++i;

	while (i != activations.rend())
	{
		deltas.push_back(std::move(delta));
		
		unsigned int layerNumber = std::distance(activations.begin(), --(i.base()));
		//util::log ("DenseBackPropagation") << " Layer number: " << layerNumber << "\n";
		auto& layer = network[layerNumber];

		network.formatOutputForLayer(layer, deltas.back());

		auto activationDerivativeOfCurrentLayer = i->sigmoidDerivative();
		auto deltaPropagatedReverse = layer.runReverse(deltas.back());
	   
		delta = deltaPropagatedReverse.elementMultiply(activationDerivativeOfCurrentLayer);

		++i; 
	}

	std::reverse(deltas.begin(), deltas.end());
	
	if(util::isLogEnabled("DenseBackPropagation::Detail"))
	{
		for(auto& delta : deltas)
		{
			util::log("DenseBackPropagation::Detail") << " added delta of size ( " << delta.rows()
				<< " ) rows and ( " << delta.columns() << " )\n" ;
			util::log("DenseBackPropagation::Detail") << " delta contains " << delta.toString() << "\n";
		}
	}
	
	return deltas;
}

BlockSparseMatrix DenseBackPropagation::getInputDelta(const NeuralNetwork& network, const BlockSparseMatrixVector& activations) const
{
	auto i = activations.rbegin();
	auto delta = (*i).subtract(*_referenceOutput).elementMultiply(i->sigmoidDerivative());
	++i;

	while (i + 1 != activations.rend())
	{
		unsigned int layerNumber = std::distance(activations.begin(), --(i.base()));
		auto& layer = network[layerNumber];

		network.formatOutputForLayer(layer, delta);

		auto activationDerivativeOfCurrentLayer = i->sigmoidDerivative();
		auto deltaPropagatedReverse = layer.runReverse(delta);

		util::log ("DenseBackPropagation") << " Computing input delta for layer number: " << layerNumber << "\n";
		delta = deltaPropagatedReverse.elementMultiply(activationDerivativeOfCurrentLayer);
		
		if(util::isLogEnabled("DenseBackPropagation::Detail"))
		{
			util::log("DenseBackPropagation::Detail") << " added delta of size ( " << delta.rows()
				<< " ) rows and ( " << delta.columns() << " )\n" ;
			util::log("DenseBackPropagation::Detail") << " delta contains " << delta.toString() << "\n";
		}

		++i; 
	}

	// Handle the first layer differently because the input does not have sigmoid applied
	unsigned int layerNumber = 0;
	auto& layer = network[layerNumber];

	network.formatOutputForLayer(layer, delta);

	auto deltaPropagatedReverse = layer.runReverse(delta);

	util::log ("DenseBackPropagation") << " Computing input delta for layer number: " << layerNumber << "\n";
	delta = deltaPropagatedReverse;
	
	if(util::isLogEnabled("DenseBackPropagation::Detail"))
	{
		util::log("DenseBackPropagation::Detail") << " added delta of size ( " << delta.rows()
			<< " ) rows and ( " << delta.columns() << " )\n" ;
		util::log("DenseBackPropagation::Detail") << " delta contains " << delta.toString() << "\n";
	}
	
	return delta;	
}

BlockSparseMatrixVector DenseBackPropagation::getActivations(const NeuralNetwork& network, const BlockSparseMatrix& input) const
{
	BlockSparseMatrixVector activations;

	activations.reserve(network.size() + 1);

	auto temp = input;

	activations.push_back(temp);
	//util::log("DenseBackPropagation") << " added activation of size ( " << activations.back().rows()
	// << " ) rows and ( " << activations.back().columns() << " )\n" ;

	for (auto i = network.begin(); i != network.end(); ++i)
	{
		network.formatInputForLayer(*i, activations.back());
	
		activations.push_back((*i).runInputs(activations.back()));
		//util::log("DenseBackPropagation") << " added activation of size ( " << activations.back().rows()
		//<< " ) rows and ( " << activations.back().columns() << " )\n" ;
	}

	util::log("DenseBackPropagation") << " intermediate stage ( " << activations[activations.size() / 2].toString() << "\n";
	util::log("DenseBackPropagation") << " final output ( " << activations.back().toString() << "\n";

	return activations;
}

static void coalesceNeuronOutputs(BlockSparseMatrix& derivative,
	const BlockSparseMatrix& skeleton)
{
	if(derivative.rowsPerBlock() == skeleton.columnsPerBlock() &&
		derivative.blocks() == skeleton.blocks()) return;
	
	// Must be evenly divisible
	assert(derivative.rows() % skeleton.columns() == 0);
	assert(derivative.columns() == skeleton.rowsPerBlock());
	
	// Add the rows together in a block-cyclic fasion
	derivative = derivative.reduceTileSumAlongRows(skeleton.columnsPerBlock(),
		skeleton.blocks());
}

BlockSparseMatrixVector DenseBackPropagation::getCostDerivative(
	const NeuralNetwork& network) const
{
	//get activations in a vector
	auto activations = getActivations(network, *_input);
	//get deltas in a vector
	auto deltas = getDeltas(network, activations);
	
	BlockSparseMatrixVector partialDerivative;

	partialDerivative.reserve(2 * deltas.size());
	
	unsigned int samples = _input->rows();

	//derivative of layer = activation[i] * delta[i+1] - for some layer i
	auto l = network.begin();
	for (auto i = deltas.begin(), j = activations.begin(); i != deltas.end() && j != activations.end(); ++i, ++j, ++l)
	{
		auto transposedDelta = (*i).transpose();
		auto& activation     = *j;
		auto& layer          = *l;

		transposedDelta.setRowSparse();
		
		util::log("DenseBackPropagation::Detail") << " computing derivative for layer " << std::distance(deltas.begin(), i) << "\n";
		util::log("DenseBackPropagation::Detail") << "  activation: " << activation.shapeString() << "\n";
		util::log("DenseBackPropagation::Detail") << "  delta-transposed: " << transposedDelta.shapeString() << "\n";

		//there will be one less delta than activation
		auto unnormalizedPartialDerivative = (transposedDelta.reverseConvolutionalMultiply(activation));
		auto normalizedPartialDerivative = unnormalizedPartialDerivative.multiply(1.0f/samples);
		
		// add in the regularization term
		auto weights = layer.getWeightsWithoutBias();

		auto lambdaTerm = weights.multiply(_lambda);
		
		// compute the derivative for the bias
		auto normalizedBiasPartialDerivative = transposedDelta.reduceSumAlongColumns().multiply(1.0f/samples);
		
		util::log("DenseBackPropagation::Detail") << "  tranposed delta: " << transposedDelta.toString() << "\n";
		util::log("DenseBackPropagation::Detail") << "  bias derivative: " << normalizedBiasPartialDerivative.toString() << "\n";
		// Account for cases where the same neuron produced multiple outputs
		//  or not enough inputs existed
		coalesceNeuronOutputs(normalizedPartialDerivative, lambdaTerm);
		coalesceNeuronOutputs(normalizedBiasPartialDerivative, layer.getBias());
	
		// Compute the partial derivatives with respect to the weights
		partialDerivative.push_back(lambdaTerm.add(normalizedPartialDerivative.transpose()));
		
		// Compute partial derivatives with respect to the bias
		partialDerivative.push_back(normalizedBiasPartialDerivative.transpose());

	}//this loop ends after all activations are done. and we don't need the last delta (ref-output anyway)

	bool performGradientChecking = util::KnobDatabase::getKnobValue("NeuralNetwork::DoGradientChecking", false);

	if (performGradientChecking)
	{ 
		float epsilon = util::KnobDatabase::getKnobValue("NeuralNetwork::GradientCheckingEpsilon", 0.05f);
		bool isInRange = gradientChecking(partialDerivative.back(), network.back(), *(++activations.rbegin()),
			*_referenceOutput, epsilon, _lambda);
		assertM(isInRange, "Gradient checking indicates gradient descent is wrong\n");
	}

	return partialDerivative;	
}

BlockSparseMatrix DenseBackPropagation::getInputDerivative(
	const NeuralNetwork& network,
	const BlockSparseMatrix& input) const
{
	//get activations in a vector
	auto activations = getActivations(network, input);
	//get deltas in a vector
	auto delta = getInputDelta(network, activations);
	
	util::log("DenseBackPropagation") << "Input delta: " << delta.toString();
	unsigned int samples = input.rows();

	auto partialDerivative = delta;

	auto normalizedPartialDerivative = partialDerivative.multiply(1.0f/samples);

	util::log("DenseBackPropagation") << "Input derivative: " << normalizedPartialDerivative.toString();

	return normalizedPartialDerivative;
}

}//end neuralnetwork

}//end minerva


