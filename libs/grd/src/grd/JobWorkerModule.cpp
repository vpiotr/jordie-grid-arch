#include "grd/JobWorkerModule.h"

// ----------------------------------------------------------------------------
// scWorkForwardHandler
// ----------------------------------------------------------------------------
class scWorkForwardHandler: public scRequestHandler {
public:
  scWorkForwardHandler(scJobWorkerModule *sender, ulong64 jobId, uint lockId, const scString &returnAddr): scRequestHandler()
    {m_returnAddr = returnAddr; m_jobId = jobId; m_lockId = lockId; m_sender = sender;};
  virtual ~scWorkForwardHandler() {};
  virtual void handleCommError(const scString &errorText, RequestPhase phase);
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
protected:
  scString m_returnAddr;
  scJobWorkerModule *m_sender;
  ulong64 m_jobId;
  uint m_lockId;
};

void scWorkForwardHandler::handleCommError(const scString &errorText, RequestPhase phase)
{
  m_sender->handleSubmitError(m_jobId, m_lockId, m_returnAddr, errorText);
}

void scWorkForwardHandler::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
  m_sender->handleSubmitSuccess(m_jobId, m_lockId, m_returnAddr);
}

void scWorkForwardHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
  scString msg = toString(a_response.getStatus())+": "+a_response.getError().getString("text", "");
  m_sender->handleSubmitError(m_jobId, m_lockId, m_returnAddr, msg);
}

// ----------------------------------------------------------------------------
// scJobWorkerModule
// ----------------------------------------------------------------------------
scJobWorkerModule::scJobWorkerModule()
{
}

scJobWorkerModule::~scJobWorkerModule()
{
}

scStringList scJobWorkerModule::supportedInterfaces() const
{
  scStringList res;
  res.push_back("job_worker");
  return res;
}

int scJobWorkerModule::handleMessage(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;

  assert(message != SC_NULL);

  if (message->getInterface() == "job_worker")
  {
    if (message->getCoreCommand() == "start_work") {
      res = handleCmdStartWork(message, response);
    }
    else if (message->getCoreCommand() == "cancel_work") {
      res = handleCmdCancelWork(message, response);
    }
  }
  return res;
}

int scJobWorkerModule::handleCmdStartWork(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();

  if (strTrim(params.getString("command", "")).length() > 0)
  {
    scString command = params.getString("command");
    res = forwardMessage(command, params, response);
  }
  return res;
}

int scJobWorkerModule::handleCmdCancelWork(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();

  if (params.hasChild("job_id"))
  {
    ulong64 jobId = params.getUInt64("job_id", 0);
    if (cancelWork(jobId))
      res = SC_MSG_STATUS_OK;
  }
  return res;
}

int scJobWorkerModule::forwardMessage(const scString &command, const scDataNode &params, scResponse &response)
{

  ulong64 jobId = params.getUInt64("job_id");
  uint lockId = params.getUInt("lock_id");
  scString returnAddrStr = params.getString("return_addr");
  scMessageAddress returnAddr(returnAddrStr);

  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand(command);
//  messageGuard->setRequestId(getScheduler()->getNextRequestId());
  messageGuard->setParams(params);

  int res = getScheduler()->dispatchMessage(*messageGuard, response);
  if (res == SC_MSG_STATUS_OK)
  {
    scDataNode &result = response.getResult();
    if (!result.hasChild("worker_addr"))
      result.addChild("worker_addr",
        new scDataNode(
          getScheduler()->getOwnAddress(
            returnAddr.getProtocol()
          ).getAsString()));
  }
  return res;
}

void scJobWorkerModule::handleSubmitError(ulong64 jobId, uint lockId, const scString &returnAddr, const scString &errorMsg)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope());
  envelopeGuard->setReceiver(scMessageAddress(returnAddr));

  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.ended");
  scDataNode params;
  params.addChild("job_id", new scDataNode(jobId));
  params.addChild("lock_id", new scDataNode(lockId));
  params.addChild("success", new scDataNode(false));
  params.addChild("error_msg", new scDataNode("forward error: "+errorMsg));

  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());

  getScheduler()->postEnvelope(envelopeGuard.release());
}

void scJobWorkerModule::handleSubmitSuccess(ulong64 jobId, uint lockId, const scString &returnAddr)
{ // empty - do nothing
}

bool scJobWorkerModule::cancelWork(ulong64 jobId)
{
  scTask *task = findJobTaskForMessage(jobId, "job_worker.cancel_work");
  bool res = (task != SC_NULL);
  if (task != SC_NULL)
    task->requestStop(); // close immediately
  return res;
}

scTask *scJobWorkerModule::findJobTaskForMessage(ulong64 jobId, const scString &message)
{
  scDataNode params;
  params.addChild("job_id", new scDataNode(jobId));
  return getScheduler()->findTaskForMessage(message, params);
}

