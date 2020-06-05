#ifndef BOOST_GENERATEAST_H
#define BOOST_GENERATEAST_H

#include <string>
#include "IR.h"
#include <set>

using namespace Boost::Internal;

class Expr_compare{
	public:
		bool operator()(const Expr &a,const Expr &b) const {
			return a.real_ptr()<b.real_ptr();
		}
	   
};

class mykernel{
public:
    Group kernel;
    std::set<Expr,Expr_compare> right_expr;
    std::vector<std::string> grad;
    mykernel(Group _kernel,std::set<Expr,Expr_compare> _right_expr,std::vector<std::string> _grad) : kernel(_kernel), right_expr(_right_expr),grad(_grad) {}
};

mykernel generateAST(std::string filename);

#endif
