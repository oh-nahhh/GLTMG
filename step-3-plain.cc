/* ---------------------------------------------------------------------
 *
 * Copyright (C) 1999 - 2016 by the deal.II authors
 *
 * This file is part of the deal.II library.
 *
 * The deal.II library is free software; you can use it, redistribute
 * it, and/or modify it under the terms of the GNU Lesser General
 * Public License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * The full text of the license can be found in the file LICENSE at
 * the top level of the deal.II distribution.
 *
 * ---------------------------------------------------------------------
 *
 * Authors: Wolfgang Bangerth, 1999,
 *          Guido Kanschat, 2011
 */
#include <deal.II/grid/tria.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/dofs/dof_tools.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/function.h>
#include <deal.II/numerics/vector_tools.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/lac/vector.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/precondition.h>
#include <deal.II/numerics/data_out.h>
#include <fstream>
#include <iostream>
#include <deal.II/base/tensor.h> // Tensor
#include <vector> // This is for the std::vector
//#include <deal.II/multigrid/mg_matrix.h> // For the mg::Matrix ;)


//#include "helloHeader.h"
//#include "helloPrecondition.h"
#include "mgPrecondition.h"

using namespace dealii;
class Step3
{
public:
  Step3 ();
  void run ();
  // Added Public methods
  void test_run(); // Here we run our tests!

  /*Johanna makes tests!:*/
  void test_outer_product(); //Test outer product with tensors!
  void test_MG(); //Test outer product with tensors!
  void presmooth_test(Vector<double> &dst, Vector<double> &src, const SparseMatrix<double> *&A);

private:
  void make_grid ();
  void setup_system ();
  void assemble_system ();
  void solve ();
  void output_results () const;
  /*Added private methods */
  void inputFile(unsigned int size1, unsigned int size2,std::string filename);
  void inputFile_supplied(unsigned int, unsigned int,std::string,SparsityPattern &sp, SparseMatrix<double> &sm);
  void writeToFile(SparseMatrix<double> &matrix,std::string filename);

  Triangulation<2>     triangulation;
  FE_Q<2>              fe;
  DoFHandler<2>        dof_handler;
  SparsityPattern      sparsity_pattern;
  SparseMatrix<double> system_matrix;
  Vector<double>       solution;
  Vector<double>       system_rhs;
};
Step3::Step3 ()
:
fe (1),
dof_handler (triangulation)
{}
void Step3::make_grid ()
{
  GridGenerator::hyper_cube (triangulation, -1, 1);
  triangulation.refine_global (5);
  std::cout << "Number of active cells: "
  << triangulation.n_active_cells()
  << std::endl;
}
void Step3::setup_system ()
{
  dof_handler.distribute_dofs (fe);
  std::cout << "Number of degrees of freedom: "
  << dof_handler.n_dofs()
  << std::endl;
  DynamicSparsityPattern dsp(dof_handler.n_dofs());
  DoFTools::make_sparsity_pattern (dof_handler, dsp);
  sparsity_pattern.copy_from(dsp);
  system_matrix.reinit (sparsity_pattern);
  solution.reinit (dof_handler.n_dofs());
  system_rhs.reinit (dof_handler.n_dofs());
}
void Step3::assemble_system ()
{
  QGauss<2>  quadrature_formula(2);
  FEValues<2> fe_values (fe, quadrature_formula,
   update_values | update_gradients | update_JxW_values);
  const unsigned int   dofs_per_cell = fe.dofs_per_cell;
  const unsigned int   n_q_points    = quadrature_formula.size();
  FullMatrix<double>   cell_matrix (dofs_per_cell, dofs_per_cell);
  Vector<double>       cell_rhs (dofs_per_cell);
  std::vector<types::global_dof_index> local_dof_indices (dofs_per_cell);
  DoFHandler<2>::active_cell_iterator cell = dof_handler.begin_active();
  DoFHandler<2>::active_cell_iterator endc = dof_handler.end();
  for (; cell!=endc; ++cell)
  {
    fe_values.reinit (cell);
    cell_matrix = 0;
    cell_rhs = 0;
    for (unsigned int q_index=0; q_index<n_q_points; ++q_index)
    {
      for (unsigned int i=0; i<dofs_per_cell; ++i){
        for (unsigned int j=0; j<dofs_per_cell; ++j){
          cell_matrix(i,j) += (fe_values.shape_grad (i, q_index) *
           fe_values.shape_grad (j, q_index) *
           fe_values.JxW (q_index));
        }
      }
      for (unsigned int i=0; i<dofs_per_cell; ++i){
        cell_rhs(i) += (fe_values.shape_value (i, q_index) *
          1 *
          fe_values.JxW (q_index));
      }
    }
    cell->get_dof_indices (local_dof_indices);
    for (unsigned int i=0; i<dofs_per_cell; ++i){
      for (unsigned int j=0; j<dofs_per_cell; ++j){
        system_matrix.add (local_dof_indices[i],
         local_dof_indices[j],
         cell_matrix(i,j));
      }
    }
    for (unsigned int i=0; i<dofs_per_cell; ++i){
      system_rhs(local_dof_indices[i]) += cell_rhs(i);
    }
  }
  std::map<types::global_dof_index,double> boundary_values;
  VectorTools::interpolate_boundary_values (dof_handler,
    0,
    ZeroFunction<2>(),
    boundary_values);
  MatrixTools::apply_boundary_values (boundary_values,
    system_matrix,
    solution,
    system_rhs);
}
void Step3::solve ()
{
      /* Original Step3::solve */ 
  SolverControl           solver_control (1000, 1e-12);
  SolverCG<>              solver (solver_control);
  solver.solve (system_matrix, solution, system_rhs,
    mgPrecondition(system_matrix,system_rhs));
  
}
void Step3::output_results () const
{
  DataOut<2> data_out;
  data_out.attach_dof_handler (dof_handler);
  data_out.add_data_vector (solution, "solution");
  data_out.build_patches ();
  std::ofstream output ("solution.gpl");
  data_out.write_gnuplot (output);
}
void Step3::run ()
{
      /* Original Step3::run */
  make_grid ();
  setup_system ();
  assemble_system ();
  solve ();
  output_results ();
}

  /*======================================== Added Methods for tests ======================================================*/

       /* Read from input file ! !
        Adjust for writing on new sparsity pattern and Sparse*/
void Step3::inputFile(unsigned int size1, unsigned int size2,std::string filename){

  DynamicSparsityPattern dsp(size1,size2);
  std::ifstream input_file(filename);
  unsigned int i,j;
  double val;

  if(!input_file){
    std::cout << "While opening file, error encounterde"<<std::endl;
  }
  else{
    std::cout << "File is successfully opened" << std::endl;
  }
  while(input_file>>i>>j>>val)//(!input_file.eof())
  {
    dsp.add(i-1,j-1);
  }
  input_file.close();
  dsp.compress();
  sparsity_pattern.copy_from(dsp);
  system_matrix.reinit(sparsity_pattern);

  std::ifstream input_file_again(filename);
  if(!input_file_again){
    std::cout << "While opening a file an error is encountered" << std::endl;
  }
  else{
    std::cout << "File is successfully opened" << std::endl;
  }
  while(input_file_again>>i>>j>>val)//(!input_file.eof())
  {
    system_matrix.add(i-1,j-1,val);
  }
  input_file_again.close();

}

void Step3::inputFile_supplied(unsigned int size1, unsigned int size2,std::string filename, 
  SparsityPattern &sp, SparseMatrix<double> &sm){

  DynamicSparsityPattern dsp(size1,size2);
  std::ifstream input_file(filename);
  unsigned int i,j;
  double val;

  if(!input_file){
    std::cout << "While opening file, error encounterde"<<std::endl;
  }
  else{
    //std::cout << "File is successfully opened" << std::endl;
  }
  while(input_file>>i>>j>>val)//(!input_file.eof())
  {
    dsp.add(i-1,j-1);
  }
  input_file.close();
  dsp.compress();
  sp.copy_from(dsp);
  sm.reinit(sp);

  std::ifstream input_file_again(filename);
  if(!input_file_again){
    std::cout << "While opening a file an error is encountered" << std::endl;
  }
  else{
    //std::cout << "File is successfully opened" << std::endl;
  }
  while(input_file_again>>i>>j>>val)//(!input_file.eof())
  {
    sm.add(i-1,j-1,val);
  }
  input_file_again.close();
}

void Step3::writeToFile(SparseMatrix<double> &matrix,std::string filename){
  // filename ex "matrix.txt"
  std::ofstream out_system (filename);
  matrix.print_formatted(out_system,3,true,0," ",1); 
}

// This public method runs our tests !
void Step3::test_run(){
  SparsityPattern spAFin,spA;
  SparseMatrix<double> AFin,A;
  int size = 125;
  inputFile_supplied(size,size,"AFin.txt",spAFin,AFin);
  int N = size; // N could be an int, AFin quadratic.
  Vector<double> y;
  y.reinit(N);
  y = 1;
  Vector<double> b(N);
  AFin.vmult(b,y);  // Have to reinit b...
  mgPrecondition mg(AFin,b);
 
}

/*======================================== Johanna makes tests ======================================================*/
void Step3::test_outer_product(){
    //Vector v();

    //Tensor<>
}
void Step3::test_MG(){
  unsigned int n=3;

  DynamicSparsityPattern dspAFin(n,n);
  dspAFin.add(0,2);
  dspAFin.add(0,0);
  dspAFin.add(1,1);
  dspAFin.add(2,2);
  SparsityPattern spAFin;
  spAFin.copy_from(dspAFin);
  SparseMatrix<double> AFin(spAFin);
  AFin.add(0,2,4.0);
  AFin.add(0,0,1.0);
  AFin.add(1,1,3.0);
  AFin.add(2,2,1.0);

  Vector<double> b(n);
  b[0]=1;
  b[1]=2;
  b[2]=3;

  mgPrecondition mg(AFin,b);

  //const
  //SparseMatrix<double> A;
  //A.copy_from(AFin);
  Vector<double> x(n);
  x=0;

  //residual test (ok)
/*  Vector<double> r(n);
  mg.newResidual(r,x,b,AFin); //residual  r=b-A*x

  std::cout<<" r0:   ";
  r.print(std::cout);
  std::cout<<"\n";

  x=1;

  mg.newResidual(r,x,b,AFin); //residual  r=b-A*x
  std::cout<<" r1:   ";
  r.print(std::cout);
  std::cout<<"\n";

  //result: residual works*/

  //presmooth test
/*  const SparseMatrix<double> *a(&AFin);
  std::vector<SparseMatrix<double> const *> BB;
  BB.push_back(a);

  mg.presmooth_test(x,b,a); //OBS: behöver matris utan nollelement på diagonalen
  std::cout<<"test av presmooth med direkt pointer ger resultat: "; //1.000e+00 6.667e-01 3.000e+00
  x.print(std::cout);
  std::cout<<"\n";

  x=0;
  mg.presmooth_test(x,b,BB[0]);
<<<<<<< HEAD
  std::cout<<"test av presmooth med pointer från std::vector ger resultat: "; //-1.100e+01 6.667e-01 3.000e+00 lolwut, måste vara att x inte resettats
=======
  std::cout<<"test av presmooth med pointer från std::vector ger resultat: "; //1.00e+0 6.667e-01 3.000e+00
>>>>>>> a7cb3804bc89d9de140f6bee56fd3b3170846ed3
  x.print(std::cout);
  std::cout<<"\n";*/

  std::vector<SparseMatrix<double> const *> BB;
  BB.push_back(&AFin);
  presmooth_test(x,b,BB[0]);
}

void Step3::presmooth_test(Vector<double> &dst,Vector<double> &src,const SparseMatrix<double> *&A){
//    void Step3::presmooth_test(Vector<double> &dst,Vector<double> &src,const SparseMatrix<double>* &A){
        A->Jacobi_step(dst,src,1);
}


/*========================================================================================================================*/

int main ()
{
  //deallog.depth_console (2);
  //Step3 laplace_problem;
  //laplace_problem.run ();
  Step3 test;
  //test.run();
  test.test_run();
  //test.test_MG();

  return 0;
}
/* - - - - - -  The code-dump - - - - - - - - - */


  /* Problem with dim, but this seems to work . . .
  std::vector<SparseMatrix<double> const *> BB;
  std::vector<SparseMatrix<double> const *>::iterator it;
  double n1 = 5;
  SparsityPattern spB;
  SparsityPattern spB2;
  const SparseMatrix<double> B = mg.prol_const(n1,spB);
  const SparseMatrix<double> B2 = mg.prol_const(n1,spB);
  BB.push_back(&B);
  BB.push_back(&B2);
  SparsityPattern spfoo;
  SparseMatrix<double> foo;
  DynamicSparsityPattern dsp(0);
  spfoo.copy_from(dsp);
  foo.reinit(spfoo);
  BB[0]->mmult(foo,*BB[1],Vector<double>(),true);
 */ 

/* This is a test of the prol method! in mgPrecondtion.h
  inputFile_supplied(3,3,"A.txt",spA,A);
  //A.print_formatted(std::cout,1,true,0," ",1);
  SparsityPattern spFoo;
  SparseMatrix<double> foo;
  std::cout<<" ======================== "<<std::endl;
  double n = 5;
  mg.prol(n,spFoo,foo);
*/

/* This is how you play with tensors*/
/*
  std::cout<<" - - - - - - - - - - - - - - - "<<std::endl;
  double foo[4] = {0, 1, 2, 3};
  Tensor<2,2,double> ten();
  int 
  std::cout<<" ten: "<<ten.memory_consumption()<<std::endl;
*/

/* This is how to initialize a matrix */
/*
  DynamicSparsityPattern dspM(0);  // Must initialize before using
  //dspM.add(1,1);
  spM.copy_from(dspM);
  M.reinit(spM);
  AFin.mmult(M,A,Vector<double>(),true);
  M.print_forma tted(std::cout,1,true,0," ",1);
*/
/* - - - - - Here I test BB and PP storages - - - - - - -*/
  /* Not sure if this is sound memory handling! ! ! 
      But this works just fine . . . . 
  std::vector<SparseMatrix<double>*> vicky;
  vicky.push_back(&A); vicky.push_back(&B);
  vicky[0]->print_formatted(std::cout,1,true,0," ",1);
      //const SparseMatrix<double> *m = system_matrix;
    //BB.push_back(m);
  */

  /* Could use MGLevelObject<Object>, which actually referenses the std::vector!
    Cant find the method to add shit :/

  MGLevelObject<SparseMatrix <double>> list();
  list.add(A);*/


  /* Part of the GLTmg Run 
  Vector<int> NN = mgTest.factor(N);
  Vector<int> nu = mgTest.unique(NN);
  Vector<int> reps = mgTest.accumVector(NN);
  std::cout<<" NN:   "; NN.print(std::cout,3,true,true);
  std::cout<<" nu:   "; nu.print(std::cout,3,true,true);
  std::cout<<" reps: "; reps.print(std::cout,3,true,true);
  double n = mgTest.vectorProd(reps,nu);
  std::cout<<"n : "<<n<<std::endl;
  */

/*
   SM.print_formatted(std::cout,1,true,0," ",1);
  writeToFile(SM,"matrix.txt");
 SparsityPattern::iterator start = SP.begin();

  system_matrix.print_formatted(std::cout,3,true,0," ",1);
  system_rhs.print(std::cout,3,true,true);
  inputFile(3,3,"bb.txt");
  mgPrecondition mgPre1(system_matrix,system_rhs);
  mgPre1.printMatrix();
  system_matrix.print_formatted(std::cout,1,true,0," ",1);
  mgPre1.printVector();

  Vector<double> test_vector;
  canITouchThis(test_vector);
  //test_vector.print(std::cout,3,true,true);
  Vector<double>::iterator iter = b.begin();
  Vector<double>::iterator ender = b.end();
  for(; iter!=ender; ++iter){
    std::cout<<*iter<<std::endl;
    *iter = 10;
  }
  b.print(std::cout,1,true,true);
  */
