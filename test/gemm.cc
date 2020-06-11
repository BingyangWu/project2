#include <string>
#include <iostream>

#include "IR.h"
#include "IRMutator.h"
#include "IRVisitor.h"
#include "IRPrinter.h"
#include "type.h"

#include <dirent.h>
#include <fstream>
#include <vector>
#include "IRCCPrinter.h"
#include "IRSumMutator.h"
#include "IRPrinter.h"
#include "GenerateAST.h"
#include "IRTermVisitor.h"
#include "BoundInfer.h"
#include "ToGrad.h"

using namespace Boost::Internal;
using namespace std;

int main() {
    const int M = 1024;
    const int N = 512;
    const int K = 256;
    Type index_type = Type::int_scalar(32);
    Type data_type = Type::float_scalar(32);

    // index i
    Expr dom_i = Dom::make(index_type, 0, M);
    Expr i = Index::make(index_type, "i", dom_i, IndexType::Spatial);

    // index j
    Expr dom_j = Dom::make(index_type, 0, N);
    Expr j = Index::make(index_type, "j", dom_j, IndexType::Spatial);

    // index k
    Expr dom_k = Dom::make(index_type, 0, K);
    Expr k = Index::make(index_type, "k", dom_k, IndexType::Reduce);

    // A
    Expr expr_A = Var::make(data_type, "A", {i, k}, {M, K});

    // B
    Expr expr_B = Var::make(data_type, "B", {k, j}, {K, N});

    // C
    Expr expr_C = Var::make(data_type, "C", {i, j}, {M, N});

    // main stmt
    Stmt main_stmt = Move::make(
        expr_C,
        Binary::make(data_type, BinaryOpType::Add, expr_C,
            Binary::make(data_type, BinaryOpType::Mul, expr_A, expr_B)),
        MoveType::MemToMem
    );

    // loop nest
    Stmt loop_nest = LoopNest::make({i, j, k}, {main_stmt});

    // kernel
    Group kernel = Kernel::make("simple_gemm", {expr_A, expr_B}, {expr_C}, {loop_nest}, KernelType::CPU);

    // visitor
    IRVisitor visitor;
    kernel.visit_group(&visitor);

    // mutator
    IRMutator mutator;
    kernel = mutator.mutate(kernel);

    // printer
    IRPrinter printer;
    std::string code = printer.print(kernel);

    std::cout << code;

    string in_dir = "../project2/cases/";
    dirent * ptr = nullptr;
    DIR * dir = opendir(in_dir.c_str());
    vector<string> files;
    while((ptr = readdir(dir)) != nullptr){
        string in_file_name(ptr->d_name);
        if(in_file_name.find(".json") != string::npos){
            mykernel kernel = generateAST(in_dir + in_file_name);

            Group updated_kernel = toGard(kernel.grad, kernel.kernel);

            vector<Stmt> updated_stmt_list;
            auto ori_kernel = updated_kernel.as<Kernel>();
            CHECK(ori_kernel, "internal error");
            for(auto i : ori_kernel->stmt_list){
                // cout << IRPrinter().print(i);
                updated_stmt_list.push_back(loop_bound_infer(i));
            }
            updated_kernel = Kernel::make(ori_kernel->name, ori_kernel->inputs, ori_kernel->outputs,
                                          updated_stmt_list, ori_kernel->kernel_type);

            IRCCPrinter printer;
            string code = printer.print(updated_kernel);

            string out_file_name = "../project2/kernels/" + 
                                   updated_kernel.as<Kernel>()->name + ".cc";

            ofstream ofile(out_file_name);
            ofile << code;
            ofile.close();
        }
    }
    closedir(dir);


    std::cout << "Success!\n";
    return 0;
}