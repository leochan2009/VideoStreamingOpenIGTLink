



/*=========================================================================
 
 Program:   Open IGT Link -- Example for Data Receiving Client Program
 Module:    $RCSfile: $
 Language:  C++
 Date:      $Date: $
 Version:   $Revision: $
 
 Copyright (c) Insight Software Consortium. All rights reserved.
 
 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notices for more information.
 
 =========================================================================*/

#ifndef __EVALUATIONTOOL_H
#define __EVALUATIONTOOL_H
#include <iostream>
#include <math.h>
#include <cstdlib>
#include <cstring>
#include <string>


class EvaluationTool
{
  public:
  
  EvaluationTool();
  ~EvaluationTool();
  
  std::string filename;
  
  std::string currentLine;
  
  void WriteCurrentLineToFile();
  
  void WriteALineToFile(std::string line);
  
  void AddAnElementToLine(std::string element);
  
};



#endif