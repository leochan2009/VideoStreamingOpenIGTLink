
#include "EvaluationTool.h"


EvaluationTool::EvaluationTool()
{
  this->currentLine = std::string("");
  this->filename = std::string("");
}

EvaluationTool::~EvaluationTool()
{
  
}

void EvaluationTool::WriteCurrentLineToFile()
{
  this->WriteALineToFile(this->currentLine);
}

void EvaluationTool::WriteALineToFile(std::string line)
{
  if(this->filename.c_str())
  {
    FILE* pYuvFile    = NULL;
    pYuvFile = fopen (this->filename.c_str(), "ab");
    if(pYuvFile)
    {
      std::string localline = std::string(line.c_str());
      localline.append("\r\n");
      fwrite(localline.c_str(), 1, localline.size(), pYuvFile);
      this->currentLine.clear();
    }
    fclose(pYuvFile);
  }
}

void EvaluationTool::AddAnElementToLine(std::string element)
{
  this->currentLine.append(element.c_str());
  this->currentLine.append(" ");
}
