/////////////////////////////////////////////////////////////////////////////
// Name:        CoreModule.cpp
// Project:     scLib
// Purpose:     Required module. Supports application execution commands.
// Author:      Piotr Likus
// Modified by:
// Created:     12/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

//wx
#include "wx/utils.h"

//sc
#include "sc/utils.h"

//grd
#include "grd/CommandParser.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

using namespace dtp;

scCommandParser::scCommandParser()
{
  initVarList();
}

scCommandParser::~scCommandParser()
{
}

void scCommandParser::setAlias(const scString &a_aliasName, const scString &a_command)
{
  m_aliasList[a_aliasName] = a_command;
}

scString scCommandParser::getAlias(const scString &a_aliasName)
{
  return m_aliasList[a_aliasName];
}

void scCommandParser::setVar(const scString &a_varName, const scString &a_value)
{
  m_varList[a_varName] = a_value;
}

scString scCommandParser::getVar(const scString &a_varName, const scString &defValue)
{
  if (isVarDefined(a_varName))     
    return m_varList[a_varName];
  else  
    return defValue;
}

scString scCommandParser::getVar(const scString &a_varName)
{
  return m_varList[a_varName];
}

bool scCommandParser::isVarDefined(const scString &a_varName)
{
  scVarList::iterator pos;
  pos = m_varList.find(a_varName);
  if (pos != m_varList.end()) 
    return true;
  else
    return false;  
}

void scCommandParser::setScheduler(const scSchedulerIntf *scheduler)
{
  m_scheduler = const_cast<scSchedulerIntf *>(scheduler);
}

scSchedulerIntf *scCommandParser::getScheduler()
{
  return m_scheduler;
}

scSchedulerIntf *scCommandParser::checkScheduler()
{
  if (m_scheduler == SC_NULL)
    throw scError("Scheduler not assigned!");
  return m_scheduler;  
}

void scCommandParser::parseCommand(const scString &a_cmd)
{
  class FoundCommand {
  public:
    scString address;
    scString command;
    scDataNode params;
    scCommandParser *owner;
    
    FoundCommand(scCommandParser *a_owner) {owner = a_owner;}
    virtual ~FoundCommand() {}
    void clear() {address = command = scString(""); params.clear();}
    void validate(const scString &text, int a_pos, int a_char) {
      if (!command.length()) 
        owner->throwSyntaxError(text, a_pos, a_char);
    }    
    bool isEmpty() {
      return (
        (address.length() == 0) 
        && 
        (command.length() == 0) 
        && 
        (params.isNull())
      );
    }
    void consumeParam(scString &name, scString &value) {      
      params.addChild(name, new scDataNode(value));
      name = value = scString("");
    }   
    void genPost(scSchedulerIntf *scheduler) {
      if ((command == scString("core.flush_events")) && (!address.length())) {
      // required to be performed immediatelly to use var in script
        owner->handleFlushEvents();
      } else {        
        if (!address.length()) address = SC_ADDR_THIS;
        scheduler->postMessage(address, command, &params);
      }
      clear();
    }    
  }; 

  if (a_cmd.length() && (a_cmd[0] == ';'))
    return; // comment line
  
#ifdef SC_LOG_CMDPARS  
  scLog::addText("Parsing command: "+a_cmd);
#endif
  
  //boost::ptr_list<FoundCommand> foundCommandList;
  boost::shared_ptr<FoundCommand> guard(new FoundCommand(this));  
  FoundCommand *newCommand = guard.get();   
  scString scanCmd = a_cmd;
  strTrimThis(scanCmd);
  replaceVars(scanCmd);
  scString paramValue, paramName;
  
  int c = 0, cp1 = 0;
  scString::size_type i, m;
  scString::size_type endPos = scanCmd.length();
  int ctx;
  int quoteChar = 0;

  enum {CTX_START, CTX_ADDR, CTX_CMD, 
    CTX_PARAM_LIST, CTX_PARAM, 
    CTX_PARAM_VAL, CTX_PARAM_VAL_QUOTED, CTX_PARAM_VAL_APOS, 
    CTX_END
  };  
      
  ctx = CTX_START;
  i = 0;
  while( (i<(int)endPos) && (ctx != CTX_END))  
  {
    c = scanCmd[i];

    if (i < scanCmd.length() - 1) cp1 = scanCmd[i + 1];
    else cp1 = 0;
        
    if (!isAscii(c)) {     
      throwSyntaxError(scanCmd, i, c); 
    }  
    if ((ctx == CTX_ADDR) && (c != ']'))
    {
      newCommand->address += char(c);
      ++i;
    }  
    else if (isSpecial(c) && (ctx != CTX_PARAM_VAL_QUOTED) && (ctx != CTX_PARAM_VAL_APOS)) 
    {
        switch (c)
        {
        case '[': 
          if (ctx != CTX_START) 
            throwSyntaxError(scanCmd, i, c);         
          ++i;  
          ctx = CTX_ADDR;
          break;
        case ']': 
          if (ctx != CTX_ADDR) 
            throwSyntaxError(scanCmd, i, c);         

          m = scanCmd.find_first_not_of(" ", i+1);
          if (m != scString::npos) {
            i = m;
            ctx = CTX_CMD;           
          } else {
            i = endPos;
            ctx = CTX_END;
          }              
          break;
        case ':': 
         switch (ctx) {
           case CTX_PARAM_LIST: case CTX_PARAM: case CTX_PARAM_VAL:
             newCommand->consumeParam(paramName, paramValue);  
             // continue below
           case CTX_CMD:        
             if (!newCommand->isEmpty()) { 
               newCommand->validate(scanCmd, i, c);
               newCommand->genPost(m_scheduler);
               newCommand->clear();
             }
             paramValue = paramName = scString("");
             ++i;
             ctx = CTX_START;
             break;
           default:
             throwSyntaxError(scanCmd, i, c); 
          } // switch ctx for ':'  
          break;
        case '"': 
        case '\'':
         switch (ctx) {
           case CTX_PARAM_LIST: case CTX_PARAM: case CTX_PARAM_VAL:
             if (c == '"')
               ctx = CTX_PARAM_VAL_QUOTED;
             else 
               ctx = CTX_PARAM_VAL_APOS;  
             quoteChar = c;  
             paramValue = "";
             ++i;
             break;             
           default:
             throwSyntaxError(scanCmd, i, c); 
          } // switch ctx for '"', '\''  
          break;
        case ',': 
         switch (ctx) {
           case CTX_PARAM_LIST: case CTX_PARAM: case CTX_PARAM_VAL:
           // new param
             newCommand->consumeParam(paramName, paramValue);  

             m = scanCmd.find_first_not_of(" ", i+1);
             if (m != scString::npos) {
               i = m;
               if (scanCmd[m] == ',') {
                 i++;
               } 
               ctx = CTX_PARAM;               
             } else {
               i = endPos;
               ctx = CTX_END; 
             }  
             break;             
           default:
             throwSyntaxError(scanCmd, i, c); 
          } // switch ctx for '"', '\''  
          break;
        case '=':
         switch (ctx) {
           case CTX_PARAM: 
             ctx = CTX_PARAM_VAL;  
             paramName = paramValue;
             paramValue = "";
             ++i;
             break;             
           default:
             throwSyntaxError(scanCmd, i, c); 
          } // switch ctx for '='  
          break;        
//---------------------------------          
        default:
          throwSyntaxError(scanCmd, i, c); 
          break;
        } // switch    
        
//---------------------------------          
// quoted string with control char
//---------------------------------          
    } else if (isSpecial(c) && ((ctx == CTX_PARAM_VAL_QUOTED) || (ctx == CTX_PARAM_VAL_APOS))) {
         switch (c) {
           case '"':
           case '\'':
             if ((c == quoteChar) && (cp1 != c))
             {
               // new param
               newCommand->consumeParam(paramName, paramValue);  
               // check what is after
               m = scanCmd.find_first_not_of(" ", i+1);
               if (m != scString::npos) {
                 if (scanCmd[m] == ',') {
                   i = m+1;
                   ctx = CTX_PARAM;
                 } else if (scanCmd[m] == ':') {
                   i = m;
                   ctx = CTX_CMD;
                 } else {
                   throwSyntaxError(scanCmd, m, c); 
                 } 
               }
               else {
                 ctx = CTX_END;
                 ++i;
               }  
               
             } else {
             // quote char inside quoted string
               if (c == cp1) {
                 paramValue += strCharToStr(char(c));
                 i += 2;
               } else {
                 paramValue += strCharToStr(char(c));
                 ++i;
               }
             }
             break;             
           default: // control char inside quoted string
             paramValue += strCharToStr(char(c));
             ++i;
             break;
          } // switch c for quoted string
    } else { 
//---------------------------------          
// not a special char
//---------------------------------          
        switch (ctx)
        {
         case CTX_START:
           newCommand->clear();
           newCommand->command += char(c);
           ctx = CTX_CMD;
           ++i;
           break;
         case CTX_ADDR:
           newCommand->address += char(c);
           ++i;
           break;
         case CTX_CMD: 
           if (c == ' ') {
             m = scanCmd.find_first_not_of(" ", i);
             if (m != scString::npos) {
               i = m;
               ctx = CTX_PARAM_LIST;           
             } else {
               i = endPos;
               ctx = CTX_END;
             }  
           } else {
             newCommand->command += char(c);
             ++i;
           }
           break;
         case CTX_PARAM_LIST: 
           paramValue = paramName = "";
           if (c != ' ')
             paramValue += char(c);
           ctx = CTX_PARAM;  
           ++i;
           break;
         case CTX_PARAM: 
         case CTX_PARAM_VAL: 
           if (c != ' ')
             paramValue += char(c);
           ++i;
           break;         
         case CTX_PARAM_VAL_QUOTED: 
         case CTX_PARAM_VAL_APOS: 
           paramValue += char(c);
           ++i;
           break;         
        default:
          throwSyntaxError(scanCmd, i, c); 
          break;
        } // switch context for non-special char  
    } // else !special
  } // while  
  
  if (paramValue.length())
  {
    newCommand->consumeParam(paramName, paramValue);  
  }
   
  if (!newCommand->isEmpty()) { 
    newCommand->validate(scanCmd, i, c);
    newCommand->genPost(m_scheduler);
    newCommand->clear();
  }  
}

void scCommandParser::throwSyntaxError(const scString &text, int a_pos, int a_char) const {
   throw scError("Command syntax error, wrong character ["+toString(a_char)+"] in command ["+text+"] at pos: "+toString(a_pos));
}

bool scCommandParser::isSpecial(int c) 
{
  switch (c)
  {
  case '[': case ']': case ':': case '"': case '\'': case ',': case '=': 
    return true;
  default:
    return false;
  }
}

bool scCommandParser::isAscii(int c)
{
  return c >= 32 && c <= 127;
}

void scCommandParser::replaceVars(scString &text)
{
  if (text.find_first_of("#") != scString::npos)
  {
    scString pattern;  
    scVarList::iterator p;

    for(p = m_varList.begin(); p != m_varList.end(); p++) {
      
      pattern = "#"+(p->first)+"#";
      //text.Replace(pattern, scString(p->second), true);
      strReplaceThis(text, pattern, scString(p->second), true);
    }
  }
}

void scCommandParser::initVarList()
{
  setVar(SC_CMDPARS_VAR_PID, toString(wxGetProcessId()));  
  updateExecVars();
}

void scCommandParser::updateExecVars()
{
  setVar(SC_CMDPARS_VAR_EXEC_DIR, calcExecDir());  
  setVar(SC_CMDPARS_VAR_EXEC_PATH, calcExecPath());  
  setVar(SC_CMDPARS_VAR_EXEC_FNAME, calcExecFName());  
}

void scCommandParser::setExecPath(const scString &path)
{
  m_execPath = path;
  updateExecVars();
}

scString scCommandParser::calcExecDir()
{
  scString res, path;
  path = calcExecPath();
  res = wxPathOnly(path);
  return res;
}

scString scCommandParser::calcExecFName()
{
  scString res, path;
  path = calcExecPath();
  res = wxFileNameFromPath(path);
  return res;
}

scString scCommandParser::calcExecPath()
{
  return m_execPath;
}


void scCommandParser::handleSetVar(scDataNode &params) 
{
  if (params.size() > 1) {
    scString varName = params.getChildren().at(0).getAsString();
    scString varValue = params.getChildren().at(1).getAsString();
    setVar(varName, varValue);
  }  
  else
    throw scError("Not enough params for set_var");
}

void scCommandParser::handleFlushEvents() 
{
  checkScheduler()->flushEvents();
}  
  