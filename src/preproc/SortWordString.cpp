#include "SortWordString.h"
#include "features/Features.h"
#include "features/StringFeatures.h"
#include <math.h>

CSortWordString::CSortWordString() : CStringPreProc<WORD>("SortWordString", "STWS") 
{
}

CSortWordString::~CSortWordString()
{
}

/// initialize preprocessor from features
bool CSortWordString::init(CFeatures* f)
{
	assert(f->get_feature_class() == C_STRING);
	assert(f->get_feature_type() == F_WORD);
	return true;
}

/// clean up allocated memory
void CSortWordString::cleanup()
{
}

/// initialize preprocessor from file
bool CSortWordString::load(FILE* f)
{
	return false;
}

/// save preprocessor init-data to file
bool CSortWordString::save(FILE* f)
{
	return false;
}

/// apply preproc on feature matrix
/// result in feature matrix
/// return pointer to feature_matrix, i.e. f->get_feature_matrix();
bool CSortWordString::apply_to_feature_strings(CFeatures* f)
{
	INT i;
	INT num_vec=((CStringFeatures<WORD>*)f)->get_num_vectors() ;
	
	for (i=0; i<num_vec; i++)
	{
		INT len = 0 ;
		WORD* vec = ((CStringFeatures<WORD>*)f)->get_feature_vector(i, len) ;
		//CIO::message(M_DEBUG, "sorting string of length %i\n", len) ;
		
		math.qsort(vec, len);
	}
	return true ;
}

/// apply preproc on single feature vector
WORD* CSortWordString::apply_to_feature_string(WORD* f, INT& len)
{
	WORD* vec=new WORD[len];
	INT i=0;

	for (i=0; i<len; i++)
		vec[i]=f[i];

	math.qsort(vec, len);

	return vec;
}

/// initialize preprocessor from file
bool CSortWordString::load_init_data(FILE* src)
{
	return true;
}

/// save init-data (like transforamtion matrices etc) to file
bool CSortWordString::save_init_data(FILE* dst)
{
	return true;
}
