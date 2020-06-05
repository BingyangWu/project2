#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <stdlib.h>
#include <string.h>
#include "IR.h"
#include "IRMutator.h"
#include "IRVisitor.h"
#include "IRPrinter.h"
#include "type.h"
#include "GenerateAST.h"

using namespace Boost::Internal;

std::map<std::string, Expr> record;
std::set<Expr,Expr_compare> right_expr;

bool isdigit(std::string s){
    for(int i=0;i<s.length();i++){
        if((s[i]-'0'<0||s[i]-'0'>9)&&s[i]!='.') return false;
    }
    return true;
}
bool isId(std::string s){
    for(int i=0;i<s.length();i++){
        if(s[i]=='+'||s[i]=='-'||s[i]=='*'||s[i]=='/'||s[i]=='%')
            return false;
    }
    return true;
}
int isRef(std::string s){
    int tmp=0;
    for(int i=0;i<s.length();i++){
        if(s[i]=='<'){
            tmp=i;
            break;
        }
    }
    std::string id=s.substr(0,tmp);
    if(!isId(id)) return 0;
    for(int i=tmp;i<s.length();i++){
        if(s[i]=='>'){
            tmp=i;
            break;
        }
    }
    if(tmp==s.length()-1) {
        return 1;//SRef
    }
    if(s[tmp+1]!='[') {
        
        return 0;
    }
    for(int i=tmp+1;i<s.length();i++){
        if(s[i]==']'){
            if(i!=s.length()-1) return 0;
        }
    }
    return 2;//TRef
}

Expr produce(std::string s,std::map<std::string,Expr> mapExpr){
    if(isdigit(s)){
        char c[20];
        strcpy(c,s.c_str());
        return IntImm::make(Type::int_scalar(32),atoi(c));
    }
    if(isId(s)){
        std::map<std::string,Expr>::iterator iter=mapExpr.find(s);
        return iter->second;
    }
    int brackets=0;
    std::vector<std::pair<std::string,int>> sig;
    for(int i=0;i<=(int)s.length()-1;i++){
        if(s[i]=='(') brackets++;
        else if(s[i]==')') brackets--;
        else if(brackets==0&&(s[i]=='+'||s[i]=='*'||s[i]=='/'||s[i]=='%')){
            std::string t="";
            if(s[i+1]!='/')
                t+=s[i];
            else{
                t+="//";
                i++;
            }
            if(s[i]=='/')
                sig.push_back(std::pair<std::string,int>(t,i-1));
            else
                sig.push_back(std::pair<std::string,int>(t,i));
        }
    }
    if(sig.empty()) return produce(s.substr(1,s.length()-2),mapExpr);
    for(std::vector<std::pair<std::string,int>>::iterator iter=sig.end()-1;iter>=sig.begin();iter--){
        if(iter->first=="+"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+1,s.length()-iter->second-1);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Add, produce(str1,mapExpr),produce(str2,mapExpr));
        }
    }
    for(std::vector<std::pair<std::string,int>>::iterator iter=sig.end()-1;iter>=sig.begin();iter--){
        if(iter->first=="*"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+1,s.length()-iter->second-1);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Mul, produce(str1,mapExpr),produce(str2,mapExpr));
        }
        if(iter->first=="//"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+2,s.length()-iter->second-2);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Div, produce(str1,mapExpr),produce(str2,mapExpr));
        }
        if(iter->first=="%"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+1,s.length()-iter->second-1);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Mod, produce(str1,mapExpr),produce(str2,mapExpr));
        }
    }
    return false;
}

Expr get_Tref(std::string s,std::map<std::string,Expr> mapExpr,Type data_type){
    std::vector<size_t> left_size;
    int mark=0;
    for(int i=0;i<s.length();i++){
        if(s[i]=='<'){
            mark=i;
            break;
        }
    }
    std::string ID=s.substr(0,mark);
    for(int i=0;i<s.length();i++){
        if(s[i]=='<'){
            int start=i+1;
            for(int k=i+1;k<s.length();k++){
                if(s[k]==','||s[k]=='>'){
                    std::string number_tmp=s.substr(start,k-start);
                    char c[20];
                    strcpy(c,number_tmp.c_str());
                    left_size.push_back(atoi(c));
                    start=k+1;
                    if(s[k]=='>') break;
                }
            }
        }
    }
    std::string NoSpace="";
    bool flag=false;
    for(int i=0;i<s.length();i++){
        if(s[i]==']'){
            break;
        }
        if(flag==true&&s[i]!=' ')  NoSpace+=s[i];
        if(s[i]=='['){
            flag=true;
        }
    }
    std::vector<Expr> Clist;
    int start=0;
    for(int i=0;i<NoSpace.length();i++){
        if(NoSpace[i]==','){
            std::string temp=NoSpace.substr(start,i-start);
            Clist.push_back(produce(temp,mapExpr));
            start=i+1;
        }
        if(i==NoSpace.length()-1){
            std::string temp=NoSpace.substr(start,i-start+1);
            Clist.push_back(produce(temp,mapExpr));
        }
    }
    Expr ret=Var::make(data_type,ID,Clist,left_size);
    if(record.empty()) record.insert(std::pair<std::string,Expr>(ID,ret));
    else{
        std::map<std::string,Expr>::iterator iter=record.find(ID);
        if(iter==record.end()){
            record.insert(std::pair<std::string,Expr>(ID,ret));
        }
    }
    return ret;
}

Expr right_produce(std::string s,std::map<std::string,Expr> mapExpr,Type data_type,std::string d_type,int level){
    if(isdigit(s)){
        int isfloat=false;
        for(int i=0;i<s.length();i++){
            if(s[i]=='.'){
                isfloat=true;
                break;
            }
        }
        char c[20];
        strcpy(c,s.c_str());
        if(isfloat)
            return FloatImm::make(Type::float_scalar(32),atof(c));
        if(d_type=="float") return FloatImm::make(Type::float_scalar(32),atof(c));
        return IntImm::make(Type::int_scalar(32),atoi(c));
    }
    if(isRef(s)==2){
        return get_Tref(s,mapExpr,data_type);
    }
    if(isRef(s)==1){
        int tmp=0;
        for(int i=0;i<s.length();i++){
            if(s[i]=='<'){
                tmp=i;
                break;
            }
        }
        std::string ID=s.substr(0,tmp);
        Expr ret=Var::make(data_type,ID,{},{1});
        if(record.empty()) record.insert(std::pair<std::string,Expr>(ID,ret));
        else{
            std::map<std::string,Expr>::iterator iter=record.find(ID);
            if(iter==record.end()){
                record.insert(std::pair<std::string,Expr>(ID,ret));
            }
        }
        return ret;
    }
    int brackets=0;
    std::vector<std::pair<std::string,int>> sig;
    for(int i=0;i<s.length();i++){
        if(s[i]=='(') brackets++;
        else if(s[i]==')') brackets--;
        else if(s[i]=='['){
            for(int j=i+1;j<s.length();j++){
                if(s[j]==']'){
                    i=j;
                    break;
                }
            }
        }
        else if(brackets==0&&(s[i]=='+'||s[i]=='-'||s[i]=='*'||s[i]=='/'||s[i]=='%')){
            if(s[i+1]!='/'){
                std::string t="";
                t+=s[i];
                sig.push_back(std::pair<std::string,int>(t,i));
            }
            else{
                sig.push_back(std::pair<std::string,int>("//",i));
                i++;
            }
        }
    }
    if(sig.empty()){
        Expr temp=right_produce(s.substr(1,s.length()-2),mapExpr,data_type,d_type,level+1);
        if(level==0){
            right_expr.insert(temp);
        }
        return temp;
    }
    for(std::vector<std::pair<std::string,int>>::iterator iter=sig.end()-1;iter>=sig.begin();iter--){
        if(iter->first=="+"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+1,s.length()-iter->second-1);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Add, right_produce(str1,mapExpr,data_type,d_type,level),right_produce(str2,mapExpr,data_type,d_type,level));
        }
        if(iter->first=="-"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+1,s.length()-iter->second-1);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Sub, right_produce(str1,mapExpr,data_type,d_type,level),right_produce(str2,mapExpr,data_type,d_type,level));
        }
    }
    for(std::vector<std::pair<std::string,int>>::iterator iter=sig.end()-1;iter>=sig.begin();iter--){
        if(iter->first=="*"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+1,s.length()-iter->second-1);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Mul, right_produce(str1,mapExpr,data_type,d_type,level),right_produce(str2,mapExpr,data_type,d_type,level));
        }
        if(iter->first=="/"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+1,s.length()-iter->second-1);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Div, right_produce(str1,mapExpr,data_type,d_type,level),right_produce(str2,mapExpr,data_type,d_type,level));
        }
        else if(iter->first=="//"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+2,s.length()-iter->second-2);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Div, right_produce(str1,mapExpr,data_type,d_type,level),right_produce(str2,mapExpr,data_type,d_type,level));
        }
        else if(iter->first=="%"){
            std::string str1=s.substr(0,iter->second);
            std::string str2=s.substr(iter->second+1,s.length()-iter->second-1);
            return Binary::make(Type::int_scalar(32), BinaryOpType::Mod, right_produce(str1,mapExpr,data_type,d_type,level),right_produce(str2,mapExpr,data_type,d_type,level));
        }
    }
    return false;
}

Group get_kernel_name(std::string name, std::string *in, std::string *out, std::string d_type, std::string *kernel,int kernel_length,int in_length,int out_length){
    Type index_type = Type::int_scalar(32);
    Type data_type;
    if(d_type=="float"){
        data_type=Type::float_scalar(32);
    }
    else if(d_type=="int"){
        data_type=Type::int_scalar(32);
    }
    std::vector<Stmt> LoopVector;
    for(int s=0;s<kernel_length;s++){
        std::map<std::string, int> mapLoopName;
        std::vector<std::string> vecLoopName;
        std::map<std::string, Expr> mapExpr;
        std::vector<Expr> vecExpr;
        std::string curkernel=kernel[s];
        for(int i=0;i<curkernel.length();i++){
            int j=i+1;
            if(curkernel[i]=='<'){
                int tmpcount=0;
                for(j=i+1;j<curkernel.length();j++){
                    if(curkernel[j]=='>') break;
                    else if(curkernel[j]==','){
                        tmpcount++;
                    }
                }
                tmpcount++;
                int* size=new int[tmpcount];
                int cal=0;int start=i+1;
                for(int k=i+1;k<=j;k++){
                    if(curkernel[k]==','||curkernel[k]=='>'){
                        std::string number_tmp=curkernel.substr(start,k-start);
                        char c[20];
                        strcpy(c,number_tmp.c_str());
                        size[cal]=atoi(c);
                        cal++;
                        start=k+1;
                    }
                }
                j++;
                if(curkernel[j]!='[') continue;
                std::string* looptemp=new std::string[tmpcount];
                int start2=j+1;
                int cal2=0;
                for(int k=j+1;k<curkernel.length();k++){
                    if(curkernel[k]==','||curkernel[k]==']'){
                        looptemp[cal2]=curkernel.substr(start2,k-start2);
                        cal2++;
                        start2=k+1;
                    }
                    if(curkernel[k]==']') break;
                }
                for(int k=0;k<tmpcount;k++){
                    int start3=0;
                    std::string lptmp="";
                    for(int m=0;m<looptemp[k].length();m++){
                        if(looptemp[k][m]!=' ')
                            lptmp+=looptemp[k][m];
                    }
                    for(int m=0;m<lptmp.length();m++){
                        if(lptmp[m]=='+'||lptmp[m]=='*'||lptmp[m]=='/'||lptmp[m]=='%'||lptmp[m]=='('||lptmp[m]==')'||m==lptmp.length()-1){
                            std::string singleId="";
                            if(m==lptmp.length()-1&&lptmp[m]!=')'){
                                singleId+=lptmp.substr(start3,m-start3+1);
                            }
                            else singleId+=lptmp.substr(start3,m-start3);
                            if(!isdigit(singleId)){
                                if (mapLoopName.empty()){
                                    mapLoopName.insert(std::pair<std::string,int>(singleId,size[k]));
                                    vecLoopName.push_back(singleId);
                                }
                                else{
                                    std::map<std::string,int>::iterator iter=mapLoopName.find(singleId);
                                    if(iter!=mapLoopName.end()){
                                        if(iter->second>size[k])
                                            iter->second=size[k];
                                    }
                                    else{
                                        mapLoopName.insert(std::pair<std::string,int>(singleId,size[k]));
                                        vecLoopName.push_back(singleId);
                                    }
                                }
                            }
                            if(lptmp[m]=='/'){
                                start3=m+2;
                                m++;
                            }
                            else{
                                start3=m+1;
                            }
                        }
                    }
                }
            }
        }
        
        for(std::map<std::string,int>::iterator it=mapLoopName.begin();it!=mapLoopName.end();it++){
            Expr dom=Dom::make(index_type,0,it->second);
            Expr ex=Index::make(index_type,it->first,dom,IndexType::Spatial);
            mapExpr.insert(std::pair<std::string,Expr>(it->first,ex));
        }
        for(std::vector<std::string>::iterator it=vecLoopName.begin();it!=vecLoopName.end();it++){
            std::map<std::string,Expr>::iterator iter=mapExpr.find(*it);
            vecExpr.push_back(iter->second);
        }
        int i;
        for(i=0;i<curkernel.length();i++)
            if(curkernel[i]=='=') break;
        std::string left=curkernel.substr(0,i);
        std::string right=curkernel.substr(i+1,curkernel.length()-i-1);
        std::string rNoSpace="";
        for(int i=0;i<right.length();i++){
            if(right[i]!=' ') rNoSpace+=right[i];
        }
        
        Expr expr_left=get_Tref(left,mapExpr,data_type);
        Expr expr_right=right_produce(rNoSpace,mapExpr,data_type,d_type,0);
        Stmt main_stmt = Move::make(expr_left,expr_right,MoveType::MemToMem);
        Stmt loop_nest = LoopNest::make(vecExpr, {main_stmt});
        LoopVector.push_back(loop_nest);
    }
    std::vector<Expr> vecin;
    std::vector<Expr> vecout;
    if(in_length>0){
        for(int i=0;i<in_length;i++){
            std::map<std::string,Expr>::iterator iter=record.find(in[i]);
            vecin.push_back(iter->second);
        }
    }
    for(int i=0;i<out_length;i++){
        std::map<std::string,Expr>::iterator iter=record.find(out[i]);
        vecout.push_back(iter->second);
    }
    Group kernels=Kernel::make(name,vecin,vecout,LoopVector,KernelType::CPU);
    return kernels;
}




mykernel generateAST(std::string filename) {
    record.erase(record.begin(),record.end());
    right_expr.clear();
    std::vector<std::string> input;
    std::fstream ifile;
    ifile.open(filename.c_str(),std::ios::in);
    while(!ifile.eof()){
        std::string temp;
        getline(ifile,temp);
        std::string temp2="";
        if(temp[0]!='\n'){
            for(int i=0;i<temp.length();i++){
                if(temp[i]!=' ')
                    temp2+=temp[i];
            }
            if(temp2.length()>2)
                input.push_back(temp2);
        }
    }
    ifile.close();
    
    std::string name=input[0];
    int index=0;
    int xedni=0;
    for(int i=0;i<input[0].length();i++){
        if(input[0][i]==':'){
            index=i;
            break;
        }
    }
    for(int i=input[0].length()-1;i>=0;i--){
        if(input[0][i]=='"'){
            xedni=i;
            break;
        }
    }
    name=name.substr(index+2,xedni-index-2);
    std::string pin=input[1];
    index=0;
    xedni=0;
    for(int i=0;i<input[1].length();i++){
        if(input[1][i]=='['){
            index=i;
            break;
        }
    }
    for(int i=input[1].length()-1;i>=0;i--){
        if(input[1][i]==']'){
            xedni=i;
            break;
        }
    }
    pin=pin.substr(index+1,xedni-index-1);
    int countin=0;
    int have=false;
    for(int i=0;i<pin.length();i++){
        if(pin[i]==','){
            countin+=1;
        }
        if(pin[i]=='"') have=true;
    }
    if(have)
        countin+=1;
    std::string *in=new std::string[countin];
    int tmp=0;
    for(int i=0;i<pin.length();i++){
        if(pin[i]=='"'){
            for(int j=i+1;j<pin.length();j++){
                if(pin[j]=='"'){
                    in[tmp]=pin.substr(i+1,j-i-1);
                    tmp++;
                    i=j;
                    break;
                }
            }
        }
    }
    std::string pout=input[2];
    index=0;
    xedni=0;
    for(int i=0;i<input[2].length();i++){
        if(input[2][i]=='['){
            index=i;
            break;
        }
    }
    for(int i=input[2].length()-1;i>=0;i--){
        if(input[2][i]==']'){
            xedni=i;
            break;
        }
    }
    pout=pout.substr(index+1,xedni-index-1);
    int countout=0;
    for(int i=0;i<pout.length();i++){
        if(pout[i]==','){
            countout+=1;
        }
    }
    countout+=1;
    std::string *out=new std::string[countout];
    tmp=0;
    for(int i=0;i<pout.length();i++){
        if(pout[i]=='"'){
            for(int j=i+1;j<pout.length();j++){
                if(pout[j]=='"'){
                    out[tmp]=pout.substr(i+1,j-i-1);
                    tmp++;
                    i=j;
                    break;
                }
            }
        }
    }
    std::string d_type=input[3];
    index=0;
    xedni=0;
    for(int i=0;i<input[3].length();i++){
        if(input[3][i]==':'){
            index=i;
            break;
        }
    }
    for(int i=input[3].length()-1;i>=0;i--){
        if(input[3][i]=='"'){
            xedni=i;
            break;
        }
    }
    d_type=d_type.substr(index+2,xedni-index-2);
    
    std::string pkernel=input[4];
    index=0;
    xedni=0;
    for(int i=0;i<input[4].length();i++){
        if(input[4][i]==':'){
            index=i;
            break;
        }
    }
    for(int i=input[4].length()-1;i>=0;i--){
        if(input[4][i]=='"'){
            xedni=i;
            break;
        }
    }
    pkernel=pkernel.substr(index+2,xedni-index-2);
    int count=0;
    for(int i=0;i<pkernel.length();i++){
        if(pkernel[i]==';'){
            count+=1;
        }
    }
    std::string *kernel=new std::string[count];
    tmp=0;
    int start=0;
    for(int i=0;i<pkernel.length();i++){
        if(pkernel[i]==';'){
            kernel[tmp]=pkernel.substr(start,i-start);
            int p=0;
            for(p=i+1;p<pkernel.length();p++){
                if(pkernel[p]==' ') continue;
                else break;
            }
            start=p;
            tmp++;
        }
    }
    std::string pgrad=input[5];
    index=0;
    xedni=0;
    for(int i=0;i<input[5].length();i++){
        if(input[5][i]=='['){
            index=i;
            break;
        }
    }
    for(int i=input[5].length()-1;i>=0;i--){
        if(input[5][i]==']'){
            xedni=i;
            break;
        }
    }
    pgrad=pgrad.substr(index+1,xedni-index-1);
    int countgrad=0;
    int havegrad=false;
    for(int i=0;i<pgrad.length();i++){
        if(pgrad[i]==','){
            countgrad+=1;
        }
        if(pgrad[i]=='"') have=true;
    }
    if(have)
        countgrad+=1;
    std::string *grad=new std::string[countgrad];
    tmp=0;
    for(int i=0;i<pgrad.length();i++){
        if(pgrad[i]=='"'){
            for(int j=i+1;j<pgrad.length();j++){
                if(pgrad[j]=='"'){
                    grad[tmp]=pgrad.substr(i+1,j-i-1);
                    tmp++;
                    i=j;
                    break;
                }
            }
        }
    }
   
    Group _root=get_kernel_name(name,in,out,d_type,kernel,count,countin,countout);
    mykernel root(_root,right_expr,grad);
    return root;
}

