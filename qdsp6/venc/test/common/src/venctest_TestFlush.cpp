/*--------------------------------------------------------------------------
Copyright (c) 2010, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Code Aurora nor
      the names of its contributors may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------------*/

/*========================================================================
  Include Files
 ==========================================================================*/
#include "venctest_ComDef.h"
#include "venctest_Debug.h"
#include "venctest_TestFlush.h"
#include "venctest_Time.h"
#include "venctest_Encoder.h"
#include "venctest_Queue.h"
#include "venctest_File.h"

namespace venctest
{
  static const OMX_S32 PORT_INDEX_IN = 0;
  static const OMX_S32 PORT_INDEX_OUT = 1;

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  TestFlush::TestFlush()
    : ITestCase(),    // invoke the base class constructor
    m_pEncoder(NULL),
    m_pInputQueue(NULL),
    m_pOutputQueue(NULL),
    m_pSource(NULL),
    m_pSink(NULL),
    m_nTimeStamp(0)
  {
  }

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  TestFlush::~TestFlush()
  {
  }

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  OMX_ERRORTYPE TestFlush::ValidateAssumptions(EncoderConfigType* pConfig,
      DynamicConfigType* pDynamicConfig)
  {
    OMX_ERRORTYPE result = OMX_ErrorNone;
    return result;
  }

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  OMX_ERRORTYPE TestFlush::Execute(EncoderConfigType* pConfig,
      DynamicConfigType* pDynamicConfig,
      OMX_S32 nTestNum)
  {
    OMX_ERRORTYPE result = OMX_ErrorNone;

    if (result == OMX_ErrorNone)
    {
      //==========================================
      // Create input buffer queue
      VENC_TEST_MSG_HIGH("Creating input buffer queue...");
      m_pInputQueue = new Queue(pConfig->nInBufferCount,
          sizeof(OMX_BUFFERHEADERTYPE*));

      //==========================================
      // Create output buffer queue
      VENC_TEST_MSG_HIGH("Creating output buffer queue...");
      m_pOutputQueue = new Queue(pConfig->nOutBufferCount,
          sizeof(OMX_BUFFERHEADERTYPE*));
    }

    //==========================================
    // Create and open yuv file
    if (pConfig->cInFileName[0] != (char) 0)
    {
      VENC_TEST_MSG_HIGH("Creating file source...");
      m_pSource = new File();
      result = CheckError(m_pSource->Open(pConfig->cInFileName, OMX_TRUE));
    }
    else
    {
      VENC_TEST_MSG_HIGH("Not reading from input file");
    }

    //==========================================
    // Create and open m4v file
    if (result == OMX_ErrorNone)
    {
      if (pConfig->cOutFileName[0] != (char) 0)
      {
        VENC_TEST_MSG_HIGH("Creating file sink...");
        m_pSink = new File();
        result = CheckError(m_pSink->Open(pConfig->cOutFileName, OMX_FALSE));
      }
      else
      {
        VENC_TEST_MSG_HIGH("Not writing to output file");
      }
    }

    //==========================================
    // Create and configure the encoder
    if (result == OMX_ErrorNone)
    {
      VENC_TEST_MSG_HIGH("Creating encoder...");
      m_pEncoder = new Encoder(EBD,
          FBD,
          this, // set the test case object as the callback app data
          pConfig->eCodec);
      result = CheckError(m_pEncoder->Configure(pConfig));
    }

    //==========================================
    // Go to executing state (also allocate buffers)
    if (result == OMX_ErrorNone)
    {
      VENC_TEST_MSG_HIGH("Go to executing state...");
      result = CheckError(m_pEncoder->GoToExecutingState());
    }

    //==========================================
    // Get the allocated input buffers
    if (result == OMX_ErrorNone)
    {
      OMX_BUFFERHEADERTYPE** ppInputBuffers;
      ppInputBuffers = m_pEncoder->GetBuffers(OMX_TRUE);
      for (int i = 0; i < pConfig->nInBufferCount; i++)
      {
        ppInputBuffers[i]->pAppPrivate = m_pEncoder; // set the encoder as the private app data
        result = CheckError(m_pInputQueue->Push(
              &ppInputBuffers[i], sizeof(ppInputBuffers[i]))); // store buffers in queue
        if (result != OMX_ErrorNone)
        {
          break;
        }
      }
    }

    //==========================================
    // Get the allocated output buffers
    if (result == OMX_ErrorNone)
    {
      OMX_BUFFERHEADERTYPE** ppOutputBuffers;
      ppOutputBuffers = m_pEncoder->GetBuffers(OMX_FALSE);
      for (int i = 0; i < pConfig->nOutBufferCount; i++)
      {
        ppOutputBuffers[i]->pAppPrivate = m_pEncoder; // set the encoder as the private app data
        result = CheckError(m_pOutputQueue->Push(
              &ppOutputBuffers[i], sizeof(ppOutputBuffers[i]))); // store buffers in queue
        if (result != OMX_ErrorNone)
        {
          break;
        }
      }
    }

    static const OMX_S32 nState = 2;
    for (OMX_S32 nState = 0; nState < 2; nState++)
    {
      static const OMX_S32 nRuns = 4;
      static const OMX_BOOL bPortMap[nRuns][2] =
      { {OMX_FALSE,  OMX_FALSE},    // no input or output buffers
        {OMX_TRUE,   OMX_FALSE},    // input buffers only
        {OMX_FALSE,  OMX_TRUE},     // output buffers only
        {OMX_TRUE,   OMX_TRUE} };   // input and output buffers
      for (OMX_S32 i = 0; i < nRuns; i++)
      {
        //==========================================
        // Deliver buffers
        VENC_TEST_MSG_HIGH("Deliver buffers %d", (int) i);
        result = CheckError(DeliverBuffers(
              bPortMap[i][0], bPortMap[i][1], pConfig));
        if (result != OMX_ErrorNone)
        {
          VENC_TEST_MSG_ERROR("failed to deliver buffers %d", (int) i);
          break;
        }

        //==========================================
        // flush
        VENC_TEST_MSG_HIGH("flushing %d", (int) i);
        result = CheckError(m_pEncoder->Flush());
        if (result != OMX_ErrorNone)
        {
          VENC_TEST_MSG_ERROR("error flushing");
          break;
        }

        //==========================================
        // make sure all buffers have been returned to us
        result = CheckError(CheckBufferQueues(
              pConfig->nInBufferCount, pConfig->nOutBufferCount));
        if (result != OMX_ErrorNone)
        {
          VENC_TEST_MSG_ERROR("missing some buffers %d", (int) i);
          break;
        }
      }

      if (result != OMX_ErrorNone)
      {
        break;
      }

      if (nState == 0)
      {
        VENC_TEST_MSG_HIGH("going to pause state...");
        result = CheckError(m_pEncoder->SetState(OMX_StatePause, OMX_TRUE));
        if (result != OMX_ErrorNone)
        {
          VENC_TEST_MSG_ERROR("error going to pause state");
          break;
        }
      }
    }

    //==========================================
    // Tear down the encoder (also deallocate buffers)
    if (m_pEncoder != NULL)
    {
      VENC_TEST_MSG_HIGH("Go to loaded state...");
      result = CheckError(m_pEncoder->GoToLoadedState());
    }

    //==========================================
    // Close the yuv file
    if (m_pSource != NULL)
    {
      result = CheckError(m_pSource->Close());
    }

    //==========================================
    // Close the m4v file
    if (m_pSink != NULL)
    {
      result = CheckError(m_pSink->Close());
    }

    //==========================================
    // Free our helper classes
    if (m_pEncoder)
      delete m_pEncoder;
    if (m_pInputQueue)
      delete m_pInputQueue;
    if (m_pOutputQueue)
      delete m_pOutputQueue;
    if (m_pSource)
      delete m_pSource;
    if (m_pSink)
      delete m_pSink;

    return result;

  }

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  OMX_ERRORTYPE TestFlush::DeliverBuffers(OMX_BOOL bDeliverInput,
      OMX_BOOL bDeliverOutput,
      EncoderConfigType* pConfig)
  {
    OMX_ERRORTYPE result = OMX_ErrorNone;

    // deliver all input buffers
    if (bDeliverInput == OMX_TRUE)
    {
      OMX_BUFFERHEADERTYPE* pBuffer;
      OMX_S32 nSize = m_pInputQueue->GetSize();
      for (OMX_S32 i = 0; i < nSize; i++)
      {
        result = CheckError(m_pInputQueue->Pop(
              (OMX_PTR) &pBuffer, sizeof(pBuffer)));
        if (result != OMX_ErrorNone)
        {
          VENC_TEST_MSG_ERROR("failed to pop input");
          break;
        }

        pBuffer->nTimeStamp = m_nTimeStamp;
        m_nTimeStamp = m_nTimeStamp + 1000000 / pConfig->nFramerate;
        pBuffer->nFilledLen = pConfig->nFrameWidth * pConfig->nFrameHeight * 3 / 2;
        result = CheckError(m_pEncoder->DeliverInput(pBuffer));
        if (result != OMX_ErrorNone)
        {
          VENC_TEST_MSG_ERROR("failed to deliver input");
          break;
        }
      }
    }

    // deliver all output buffers
    if (bDeliverOutput == OMX_TRUE)
    {
      OMX_BUFFERHEADERTYPE* pBuffer;
      OMX_S32 nSize = m_pOutputQueue->GetSize();
      for (OMX_S32 i = 0; i < nSize; i++)
      {
        result = CheckError(m_pOutputQueue->Pop(
              (OMX_PTR) &pBuffer, sizeof(pBuffer)));
        if (result != OMX_ErrorNone)
        {
          VENC_TEST_MSG_ERROR("failed to pop output");
          break;
        }

        result = CheckError(m_pEncoder->DeliverOutput(pBuffer));
        if (result != OMX_ErrorNone)
        {
          VENC_TEST_MSG_ERROR("failed to deliver output");
          break;
        }
      }
    }

    return result;
  }

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  OMX_ERRORTYPE TestFlush::CheckBufferQueues(OMX_S32 nInputBuffers,
      OMX_S32 nOutputBuffers)
  {
    OMX_S32 inSize = m_pInputQueue->GetSize();
    OMX_S32 outSize = m_pOutputQueue->GetSize();

    VENC_TEST_MSG_HIGH("queuedInput = %ld, queuedOutput = %ld", inSize, outSize);

    if (inSize != nInputBuffers || outSize != nOutputBuffers)
    {
      VENC_TEST_MSG_ERROR("missing buffers %ld %ld", inSize, outSize);
    }
    return (inSize == nInputBuffers &&
        outSize == nOutputBuffers) ? OMX_ErrorNone : OMX_ErrorUndefined;
  }

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  OMX_ERRORTYPE TestFlush::EBD(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
  {
    VENC_TEST_MSG_HIGH("queuing input buffer with size %ld", pBuffer->nFilledLen);
    return ((TestFlush*) pAppData)->m_pInputQueue->Push(&pBuffer, sizeof(pBuffer));
  }

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  OMX_ERRORTYPE TestFlush::FBD(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
  {
    VENC_TEST_MSG_HIGH("queuing output buffer with size %ld", pBuffer->nFilledLen);
    return ((TestFlush*) pAppData)->m_pOutputQueue->Push(&pBuffer, sizeof(pBuffer));
  }

} // namespace venctest
