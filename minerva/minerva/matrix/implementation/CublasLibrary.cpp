/*	\file   CublasLibrary.cpp
	\date   Thursday August 15, 2013
	\author Gregory Diamos <solusstultus@gmail.com>
	\brief  The source file for the CublasLibrary class.
*/

// Minerva Includes
#include <minerva/matrix/interface/CublasLibrary.h>
#include <minerva/util/interface/Casts.h>

// Standard Library Includes
#include <stdexcept>

// System-Specific Includes
#include <dlfcn.h>

namespace minerva
{

namespace matrix
{

void CublasLibrary::load()
{
	_interface.load();
}

bool CublasLibrary::loaded()
{
	return _interface.loaded();
}

void CublasLibrary::cublasSgeam(
    cublasOperation_t transa, cublasOperation_t transb,
    int m, int n, const float *alpha, const float *A,
	int lda, const float *beta, const float *B, int ldb, 
	float *C, int ldc)
{
	_check();
	
	util::log("CublasLibrary") << " CUBLAS SGEAM: ("
		"handle: " << _interface.handle <<  ", "
		"transa: " << transa <<  ", "
		"transb: " << transb <<  ", "

		"m: " << m <<  ", "
		"n: " << n <<  ", "

		"alpha: " << alpha <<  " (" << *alpha << "), "
		"A: " << A <<  ", "
		"lda: " << lda <<  ", "

		"beta: " << beta <<  " (" << *beta << "), "
		"B: " << B <<  ", "
		"ldb: " << ldb <<  ", "

		"C: " << C <<  ", "
		"ldc: " << ldc << ")\n";
	
	cublasStatus_t status = (*_interface.cublasSgeam)(_interface.handle,
		transa, transb, m, n, alpha, A, lda, beta, B, ldb, C, ldc);
		
	if(status != CUBLAS_STATUS_SUCCESS)
	{
		throw std::runtime_error("Cuda SGEAM failed: " +
			cublasGetErrorString(status));
	}

}

void CublasLibrary::cublasSgemm(
	cublasOperation_t transa, cublasOperation_t transb,
	int m, int n, int k, const float* alpha, const float *A, int lda, 
	const float *B, int ldb, const float* beta, float *C, int ldc)
{
	_check();
	
	util::log("CublasLibrary") << " CUBLAS SGEMM: ("
		"handle: " << _interface.handle <<  ", "
		"transa: " << transa <<  ", "
		"transb: " << transb <<  ", "

		"m: " << m <<  ", "
		"n: " << n <<  ", "
		"k: " << k <<  ", "

		"alpha: " << alpha <<  " (" << *alpha << "), "
		"A: " << A <<  ", "
		"lda: " << lda <<  ", "

		"B: " << B <<  ", "
		"ldb: " << ldb <<  ", "
		"beta: " << beta <<  " (" << *beta << "), "

		"C: " << C <<  ", "
		"ldc: " << ldc << ")\n";
	
	cublasStatus_t status = (*_interface.cublasSgemm_v2)(_interface.handle,
		transa, transb, m, n, k, alpha, A, lda, B, ldb,
		beta, C, ldc);
		
	if(status != CUBLAS_STATUS_SUCCESS)
	{
		throw std::runtime_error("Cuda SGEMM failed: " +
			cublasGetErrorString(status));
	}
}

void* CublasLibrary::cudaMalloc(size_t bytes)
{
	_check();

	void* address = nullptr;

	int status = (*_interface.cudaMalloc)(&address, bytes);
	
	if(status != CUDA_SUCCESS)
	{
		throw std::runtime_error("Cuda malloc failed: " +
			cudaGetErrorString(status));
	}
	
	util::log("CublasLibrary") << " CUDA allocated memory (address: "
		<< address << ", " << bytes << " bytes)\n";
		
	return address;
}

void CublasLibrary::cudaFree(void* ptr)
{
	_check();

	(*_interface.cudaFree)(ptr);

	util::log("CublasLibrary") << " CUDA freed memory (address: "
		<< ptr << ")\n";
}

void CublasLibrary::cudaMemcpy(void* dest, const void* src, size_t bytes, 
	cudaMemcpyKind kind)
{
	_check();

	util::log("CublasLibrary") << " CUDA memcopy (destination address: "
		<< dest << ", source address: " << src << ", " << bytes
		<< " bytes)\n";

	int status = (*_interface.cudaMemcpy)(dest, src, bytes, kind);

	if(status != CUDA_SUCCESS)
	{
		throw std::runtime_error("Cuda memcpy failed: " +
			cudaGetErrorString(status));
	}
}

std::string CublasLibrary::cudaGetErrorString(int error)
{
	_check();
	
	return (*_interface.cudaGetErrorString)(error);
}

std::string CublasLibrary::cublasGetErrorString(cublasStatus_t error)
{
	switch(error)
	{
	case CUBLAS_STATUS_SUCCESS:
	{
		return "success";
		break;
	}
	case CUBLAS_STATUS_NOT_INITIALIZED:
	{
		return "not initialized";
		break;
	}
	case CUBLAS_STATUS_ALLOC_FAILED:
	{
		return "allocation failed";
		break;
	}
	case CUBLAS_STATUS_INVALID_VALUE:
	{
		return "invalid value";
		break;
	}
	case CUBLAS_STATUS_ARCH_MISMATCH:
	{
		return "arch mismatch";
		break;
	}
	case CUBLAS_STATUS_MAPPING_ERROR:
	{
		return "mapping error";
		break;
	}
	case CUBLAS_STATUS_EXECUTION_FAILED:
	{
		return "execution failed";
		break;
	}
	case CUBLAS_STATUS_INTERNAL_ERROR:
	{
		return "internal error";
		break;
	}
	}
	
	return "Unknown error.";
}

void CublasLibrary::_check()
{
	load();
	
	if(!loaded())
	{
		throw std::runtime_error("Tried to call CUBLAS function when "
			"the library is not loaded. Loading library failed, consider "
			"installing CUBLAS.");
	}
}

CublasLibrary::Interface::Interface()
: handle(nullptr), _library(nullptr), _failed(false)
{

}

CublasLibrary::Interface::~Interface()
{
	unload();
}

static void checkFunction(void* pointer, const std::string& name)
{
	if(pointer == nullptr)
	{
		throw std::runtime_error("Failed to load function '" + name +
			"' from dynamic library.");
	}
}

void CublasLibrary::Interface::load()
{
	if(_failed)  return;
	if(loaded()) return;
	
    #ifdef __APPLE__
    //const char* libraryName = "libcublas.dylib";
    const char* libraryName = "libcublas-optimized.dylib";
    #else
    const char* libraryName = "libcublas.so";
    #endif

	_library = dlopen(libraryName, RTLD_LAZY);

    util::log("CublasLibrary") << "Loading library '" << libraryName << "'\n";

    if(!loaded())
	{
		util::log("CublasLibrary") << " Failed to load library '" << libraryName
			<< "'\n";
		_failed = true;
		return;
	}

	try
	{
	
		#define DynLink( function ) \
			util::bit_cast(function, dlsym(_library, #function)); \
			checkFunction((void*)function, #function)
			
		DynLink(cublasSgeam);
		DynLink(cublasSgemm_v2);
		
		DynLink(cublasCreate_v2);
		DynLink(cublasDestroy_v2);
			
		DynLink(cudaMalloc);
		DynLink(cudaFree);
		DynLink(cudaMemcpy);
		DynLink(cudaGetErrorString);
		
		#undef DynLink	

		util::log("CublasLibrary") << " Loaded library '" << libraryName
			<< "' successfully\n";
			
		_createHandle();
	}
	catch(...)
	{
		unload();
		throw;
	}
}

bool CublasLibrary::Interface::loaded() const
{
	return _library != nullptr;
}

void CublasLibrary::Interface::unload()
{
	if(!loaded()) return;

	//_destroyHandle();

	dlclose(_library);
	_library = nullptr;
}

void CublasLibrary::Interface::_createHandle()
{
	cublasStatus_t status = (*cublasCreate_v2)(&handle);

	if(status != CUBLAS_STATUS_SUCCESS)
	{
		throw std::runtime_error("Failed to create cublas handle.");
	}
}

void CublasLibrary::Interface::_destroyHandle()
{
	cublasStatus_t status = (*cublasDestroy_v2)(handle);

	assert(status == CUBLAS_STATUS_SUCCESS);
}

CublasLibrary::Interface CublasLibrary::_interface;

}

}


