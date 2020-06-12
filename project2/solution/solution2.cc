#include <string>
#include <iostream>
#include <dirent.h>
#include <fstream>
#include <vector>

#include "IRCCPrinter.h"
#include "IRIOMutator.h"
#include "GenerateAST.h"
#include "BoundInfer.h"
#include "ToGrad.h"

using namespace Boost::Internal;
using namespace std;

int main() {
    string in_dir = "../project2/cases/";
    dirent * ptr = nullptr;
    DIR * dir = opendir(in_dir.c_str());
    vector<string> files;
    while((ptr = readdir(dir)) != nullptr){
        string in_file_name(ptr->d_name);
        if(in_file_name.find(".json") != string::npos){
            mykernel kernel = generateAST(in_dir + in_file_name);

            Group updated_kernel = toGrad(kernel.grad, kernel.kernel);

            IRIOMutator mutator;
            updated_kernel = mutator.mutate(updated_kernel);

            vector<Stmt> updated_stmt_list;
            auto ori_kernel = updated_kernel.as<Kernel>();
            CHECK(ori_kernel, "internal error");
            for(auto i : ori_kernel->stmt_list){
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
    return 0;
}