/*!
 *  \copyright This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *  \author Sivaram Ambikasaran, Ruoxi Wang
 *  \version 3.1
 */
/*!	\file FLIPACK_textfile_mykernel.cpp
 Input type: Text file;
 Kernel type: kernel defined by user.
*/
#include"environment.hpp"
#include"FLIPACK_Header.hpp"
#include"BBFMM2D.hpp"


using namespace std;
using namespace Eigen;

/*! Deifne user's own kernel */
class myKernel: public kernel_Base {
public:
    virtual double kernel_Func(Point r0, Point r1){
        //implement your own kernel here
        double rSquare	=	(r0.x-r1.x)*(r0.x-r1.x) + (r0.y-r1.y)*(r0.y-r1.y);
        return exp(-pow(pow(rSquare,0.5)/900.0,0.5));
    }
};

int main(){
    /**********************************************************/
    /*                                                        */
    /*              Initializing the problem                  */
    /*                                                        */
    /**********************************************************/
    
    cout << endl << "INITIALIZING THE PROBLEM..." << endl;

    clock_t start   =   clock();
    
    /*******            Getting meta data          ******/
    
    unsigned long N;            //  Number of unknowns;
    unsigned m;                 //  Number of measurements;
    unsigned nMeasurementSets;  //  Number of measurement sets;
    unsigned short p;           //  Number of terms in the structure;
    
    string filenameMetadata     =   "../input/metadatatext.txt";
    read_Metadata (filenameMetadata, N, p, m, nMeasurementSets);

    /*******    Getting location and Htranspose   *******/
	
    vector<Point> location; //  Location of the unknowns;
    double* Htranspose;     //  Transpose of the measurement operator;
    Htranspose  =   new double[N*m]; 
    
    string filenameLocationHtranpose = "./../input/test_Location_H.txt";
    
    read_Location_Charges (filenameLocationHtranpose.c_str(),  N,location,  m, Htranspose);

    
    cout << endl << "Number of unknowns is: "     << N << endl;
    cout << endl << "Number of measurements is: " << m << endl;
    cout << endl << "Number of sets of measurements is: " << nMeasurementSets << endl;
    cout << endl << "Number of terms in the structure is: " << p << endl;



    /*******     Getting X, R and measurements     *******/
    
    double* measurements;     //  Actual measurements;
    double* R;                //  Covariance matrix;
    double* X;                //  Structure of the mean;
    
    measurements = new double[m*nMeasurementSets];
    R            = new double[m*m];
    X            = new double[N*p]; 
    
    string filenameXRMeasurements = "./../input/test_X_R_Measurements.txt";
    
    read_X_R_Measurements(filenameXRMeasurements.c_str(),N,p,m,nMeasurementSets,X,R,measurements);
        
    /***  Getting the number of Chebyshev nodes for the fmm  ***/
    
    unsigned short nChebNode = 8;   //  Number of Chebyshev nodes( >= 3)
                                    //  per dimension;
    
    cout << endl << "Number of Chebyshev nodes along one direction is: " << nChebNode << endl;

    clock_t end   =   clock();
    
    double timeInitialize  =   double(end-start)/double(CLOCKS_PER_SEC);
    
    cout << endl << "Time taken to initialize the problem is: " << timeInitialize << endl;
    
    /**********************************************************/
    /*                                                        */
    /*                 Fast Linear inversion                  */
    /*                                                        */
    /**********************************************************/
    
    cout << endl << "PERFORMING FAST LINEAR INVERSION..." << endl;
    
    start   =   clock();
    H2_2D_Tree Atree(nChebNode, Htranspose, location, N, m);// Build the fmm tree;
    /* Options of kernel:
     LOGARITHM:          kernel_Logarithm
     ONEOVERR2:          kernel_OneOverR2
     GAUSSIAN:           kernel_Gaussian
     QUADRIC:            kernel_Quadric
     INVERSEQUADRIC:     kernel_InverseQuadric
     THINPLATESPLINE:    kernel_ThinPlateSpline
     */
    
    FLIPACK<myKernel> A(Htranspose, X, measurements, R, N, m, p, nMeasurementSets, &Atree);
    
    A.compute_Solution();
    
    end   =   clock();
    
    /****     If you want to use more than one kernels    ****/
    
    /* FLIPACK<kernel_Gaussian> C(Htranspose, X, measurements, R, N, m, p, nMeasurementSets, &Atree);
     
     C.compute_Solution();*/
    
    double timeFastMethod =   double(end-start)/double(CLOCKS_PER_SEC);
    
    /****           write data into binary file          ****/
    double* solution;
    A.get_Solution(solution);
    string outputfilename = "../output/result.bin";
    write_Into_Binary_File(outputfilename, solution, N*nMeasurementSets);

    cout << endl << "Time taken for the fast method is: " << timeFastMethod << endl;

    /**********************************************************/
    /*                                                        */
    /*              Conventional Linear inversion             */
    /*                                                        */
    /**********************************************************/

    cout << endl << "PERFORMING CONVENTIONAL LINEAR INVERSION..." << endl;
    
    start   =   clock();
    
    MatrixXd Q;
    
    myKernel B; // Make sure the type of B here
                // corresponds to the kernel used
                // to generate Q.
    
    B.kernel_2D(N, location, N, location, Q);
    
    MatrixXd Htranspose_    =  Map<MatrixXd>(Htranspose, N, m);
    MatrixXd R_             =  Map<MatrixXd>(R, m, m);
    MatrixXd X_             =  Map<MatrixXd>(X, N, p);
    MatrixXd measurements_  =  Map<MatrixXd>(measurements, m, nMeasurementSets);
    
    MatrixXd temp(m+p,m+p);
    temp.block(0,0,m,m) =   Htranspose_.transpose()*Q*Htranspose_+R_;
    temp.block(0,m,m,p) =   Htranspose_.transpose()*X_;
    temp.block(m,0,p,m) =   X_.transpose()*Htranspose_;
    temp.block(m,m,p,p) =   MatrixXd::Zero(p,p);
    
    
    MatrixXd temprhs(m+p,nMeasurementSets);
    temprhs.block(0,0,m,nMeasurementSets)   =   measurements_;
    temprhs.block(m,0,p,nMeasurementSets)   =   MatrixXd::Zero(p,nMeasurementSets);
    MatrixXd tempSolution   =   temp.fullPivLu().solve(temprhs);
    
    MatrixXd finalSolution  =   X_*tempSolution.block(m,0,p,nMeasurementSets)+Q*Htranspose_*tempSolution.block(0,0,m,nMeasurementSets);
    
    end   =   clock();
    
    double  timeExactMethod    =   double(end-start)/double(CLOCKS_PER_SEC);
    
    MatrixXd solution_ =   Map<MatrixXd>(solution, N, nMeasurementSets);
    
    cout << endl << "Time taken for the exact method is: " << timeExactMethod << endl;
    
    cout << endl << "Relative difference between the fast and conventional solution is: " << (finalSolution-solution_).norm()/finalSolution.norm() << endl << endl;
    
    /*******        Clean Up        *******/
    
    delete [] Htranspose;
    delete [] measurements;
    delete [] R;
    delete [] X;
}