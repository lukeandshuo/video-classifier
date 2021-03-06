/*! \file   Constraint.h
	\author Gregory Diamos <gregory.diamos@gmail.com>
	\date   Sunday March 9, 2014
	\brief  The header file for the Constrain class.
*/

#pragma once

// Forward Declarations
namespace minerva { namespace matrix { class Matrix;            } }
namespace minerva { namespace matrix { class BlockSparseMatrix; } }

namespace minerva
{

namespace optimizer
{

/*! \brief A constraint for an optimization problem */
class Constraint
{
public:
	typedef matrix::Matrix            Matrix;
	typedef matrix::BlockSparseMatrix BlockSparseMatrix;

public:
	virtual ~Constraint();

public:
	/*! \brief Does the solution satisfy the specified constraint */
	virtual bool isSatisfied(const Matrix& ) const = 0; 

public:
	virtual void apply(BlockSparseMatrix& ) const = 0;

public:
	virtual Constraint* clone() const = 0;

};

}

}



