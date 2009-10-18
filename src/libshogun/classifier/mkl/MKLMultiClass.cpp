/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Written (W) 2009 Alexander Binder
 * Copyright (C) 2009 Fraunhofer Institute FIRST and Max-Planck-Society
 */

#include "classifier/mkl/MKLMultiClass.h"
#include "lib/io.h"

using namespace shogun;


CMKLMultiClass::CMKLMultiClass()
: CMultiClassSVM(ONE_VS_REST)
{
	svm=NULL;
	lpw=NULL;

	mkl_eps=0.01;
	max_num_mkl_iters=999;
}

CMKLMultiClass::CMKLMultiClass(float64_t C, CKernel* k, CLabels* lab)
: CMultiClassSVM(ONE_VS_REST, C, k, lab)
{
	svm=NULL;
	lpw=NULL;
	
	mkl_eps=0.01;
	max_num_mkl_iters=999;

}


CMKLMultiClass::~CMKLMultiClass()
{
	SG_UNREF(svm);
	svm=NULL;
	delete lpw;
	lpw=NULL;
}

CMKLMultiClass::CMKLMultiClass( const CMKLMultiClass & cm)
: CMultiClassSVM(ONE_VS_REST)
{
	svm=NULL;
	lpw=NULL;
	SG_ERROR(
			" CMKLMultiClass::CMKLMultiClass(const CMKLMultiClass & cm): must "
			"not be called, glpk structure is currently not copyable");
}

CMKLMultiClass CMKLMultiClass::operator=( const CMKLMultiClass & cm)
{
		SG_ERROR(
			" CMKLMultiClass CMKLMultiClass::operator=(...): must "
			"not be called, glpk structure is currently not copyable");
	return (*this);
}


void CMKLMultiClass::initsvm()
{
	if (!labels)	
	{
		SG_ERROR("CMKLMultiClass::initsvm(): the set labels is NULL\n");
	}

	SG_UNREF(svm);
	svm=new CGMNPSVM;
	SG_REF(svm);

	svm->set_C(get_C1(),get_C2());
	svm->set_epsilon(epsilon);

	int32_t numlabels;
	float64_t * lb=labels->get_labels ( numlabels);

	if (numlabels<=0)	
	{
		SG_ERROR("CMKLMultiClass::initsvm(): the number of labels is "
				"nonpositive, do not know how to handle this!\n");
	}

	CLabels* newlab=new CLabels(lb, labels->get_num_labels() );
	delete[] lb;
	lb=NULL;

	svm->set_labels(newlab);

	newlab=NULL;
}

void CMKLMultiClass::initlpsolver()
{
	if (!kernel)	
	{
		SG_ERROR("CMKLMultiClass::initlpsolver(): the set kernel is NULL\n");
	}

	if (kernel->get_kernel_type()!=K_COMBINED)
	{
		SG_ERROR("CMKLMultiClass::initlpsolver(): given kernel is not of type"
				" K_COMBINED %d required by Multiclass Mkl \n",
				kernel->get_kernel_type());
	}

	int numker=dynamic_cast<CCombinedKernel *>(kernel)->get_num_subkernels();

	ASSERT(numker>0);
	/*
	if (lpw)
	{
		delete lpw;
	}
	*/
	lpw=new MKLMultiClass2glpk;
	lpw->setup(numker);
}


bool CMKLMultiClass::evaluatefinishcriterion(const int32_t
		numberofsilpiterations)
{
	if ( (max_num_mkl_iters>0) && (numberofsilpiterations>=max_num_mkl_iters) )
	{
		return(true);
	}

	if (weightshistory.size()>1)
	{
		std::vector<float64_t> wold,wnew;

		wold=weightshistory[ weightshistory.size()-2 ];
		wnew=weightshistory.back();
		float64_t delta=0;

		ASSERT (wold.size()==wnew.size());

		for (size_t i=0;i< wnew.size();++i)
		{
			delta+=(wold[i]-wnew[i])*(wold[i]-wnew[i]);
		}
		delta=sqrt(delta);

		if( (delta < mkl_eps) && (numberofsilpiterations>=1) )
		{
			return(true);
		}
	}

	return(false);
}

void CMKLMultiClass::addingweightsstep( const std::vector<float64_t> &
		curweights)
{

	if (weightshistory.size()>2)
	{
		weightshistory.erase(weightshistory.begin());
	}

	float64_t* weights(NULL);
	weights=new float64_t[curweights.size()];
	std::copy(curweights.begin(),curweights.end(),weights);

	kernel->set_subkernel_weights(  weights, curweights.size());
	delete[] weights;
	weights=NULL;

	initsvm();

	svm->set_kernel(kernel);
	svm->train();

	float64_t sumofsignfreealphas=getsumofsignfreealphas();
	int32_t numkernels=
			dynamic_cast<CCombinedKernel *>(kernel)->get_num_subkernels();


	std::vector<float64_t> normw2(numkernels);
	for (int32_t ind=0; ind < numkernels; ++ind )
	{
		normw2[ind]=getsquarenormofprimalcoefficients( ind );
	}

	lpw->addconstraint(normw2,sumofsignfreealphas);
}

float64_t CMKLMultiClass::getsumofsignfreealphas()
{

	std::vector<int> trainlabels2(labels->get_num_labels());
	int32_t tmpint;
	int32_t * lab=labels->get_int_labels ( tmpint);
	std::copy(lab,lab+labels->get_num_labels(), trainlabels2.begin());
	delete[] lab;
	lab=NULL;


	ASSERT (trainlabels2.size()>0);
	float64_t sum=0;

	for (int32_t nc=0; nc< labels->get_num_classes();++nc)
	{
		CSVM * sm=svm->get_svm(nc);

		float64_t bia=sm->get_bias();
		sum+= bia*bia;

		SG_UNREF(sm);
	}

	::std::vector< ::std::vector<float64_t> > basealphas;
	svm->getbasealphas( basealphas);

	for (size_t lb=0; lb< trainlabels2.size();++lb)
	{
		for (int32_t nc=0; nc< labels->get_num_classes();++nc)
		{
			CSVM * sm=svm->get_svm(nc);

			if ((int)nc!=trainlabels2[lb])
			{
				CSVM * sm2=svm->get_svm(trainlabels2[lb]);

				float64_t bia1=sm2->get_bias();
				float64_t bia2=sm->get_bias();
				SG_UNREF(sm2);

				sum+= -basealphas[nc][lb]*(bia1-bia2-1);
			}
			SG_UNREF(sm);
		}
	}

	return(sum);
}

float64_t CMKLMultiClass::getsquarenormofprimalcoefficients(
		const int32_t ind)
{
	CKernel * ker=dynamic_cast<CCombinedKernel *>(kernel)->get_kernel(ind);

	float64_t tmp=0;

	for (int32_t classindex=0; classindex< labels->get_num_classes();
			++classindex)
	{
		CSVM * sm=svm->get_svm(classindex);

		for (int32_t i=0; i < sm->get_num_support_vectors(); ++i)
		{
			float64_t alphai=sm->get_alpha(i);
			int32_t svindi= sm->get_support_vector(i); 

			for (int32_t k=0; k < sm->get_num_support_vectors(); ++k)
			{
				float64_t alphak=sm->get_alpha(k);
				int32_t svindk=sm->get_support_vector(k);

				tmp+=alphai*ker->kernel(svindi,svindk)
				*alphak;

			}
		}
		SG_UNREF(sm);
	}
	SG_UNREF(ker);
	ker=NULL;

	return(tmp);
}


bool CMKLMultiClass::train(CFeatures* data)
{
	//makes glpk quiet
	glp_term_out(GLP_OFF);

	int numcl=labels->get_num_classes();
	ASSERT(kernel);
	ASSERT(labels && labels->get_num_labels());

	if (data)
	{
		if (labels->get_num_labels() != data->get_num_vectors())
			SG_ERROR("Number of training vectors does not match number of "
					"labels\n");
		kernel->init(data, data);
	}

	initlpsolver();

	weightshistory.clear();

	int32_t numkernels=
			dynamic_cast<CCombinedKernel *>(kernel)->get_num_subkernels();

	::std::vector<float64_t> curweights(numkernels,1.0/numkernels);
	weightshistory.push_back(curweights);

	addingweightsstep(curweights);

	int32_t numberofsilpiterations=0;
	bool final=false;
	while (!final)
	{

		curweights.clear();
		lpw->computeweights(curweights);
		weightshistory.push_back(curweights);


		final=evaluatefinishcriterion(numberofsilpiterations);
		++numberofsilpiterations;

		addingweightsstep(curweights);

	} // while(false==final)


	//set alphas, bias, support vecs
	ASSERT(numcl>=1);
	create_multiclass_svm(numcl);

	for (int32_t i=0; i<numcl; i++)
	{
		CSVM* osvm=svm->get_svm(i);
		CSVM* nsvm=new CSVM(osvm->get_num_support_vectors());

		for (int32_t k=0; k<osvm->get_num_support_vectors() ; k++)
		{
			nsvm->set_alpha(k, osvm->get_alpha(k) );
			nsvm->set_support_vector(k,osvm->get_support_vector(k) );
		}
		nsvm->set_bias(osvm->get_bias() );
		set_svm(i, nsvm);

		SG_UNREF(osvm);
		osvm=NULL;
	}

	SG_UNREF(svm);
	svm=NULL;
	if (lpw)
	{
		delete lpw;
	}
	lpw=NULL;
	return(true);
}




float64_t* CMKLMultiClass::getsubkernelweights(int32_t & numweights)
{
	if ( weightshistory.empty() )
	{
		numweights=0;
		return NULL;
	}

	std::vector<float64_t> subkerw=weightshistory.back();
	numweights=weightshistory.back().size();

	float64_t* res=new float64_t[numweights];
	std::copy(weightshistory.back().begin(), weightshistory.back().end(),res);
	return res;
}

void CMKLMultiClass::set_mkl_epsilon(float64_t eps )
{
	mkl_eps=eps;
}

void CMKLMultiClass::set_max_num_mkliters(int32_t maxnum)
{
	max_num_mkl_iters=maxnum;
}
