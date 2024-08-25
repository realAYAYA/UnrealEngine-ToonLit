// Copyright Epic Games, Inc. All Rights Reserved.

package com.epicgames.unreal.psoservices;

import android.app.Service;
import android.content.Intent;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLExt;
import android.opengl.EGLSurface;
import android.opengl.GLES31;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;
import android.os.Trace;
import androidx.annotation.Nullable;
import android.os.ParcelFileDescriptor;

import com.epicgames.unreal.Logger;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Date;
import java.util.UUID;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.logging.FileHandler;
import java.util.logging.Formatter;
import java.util.logging.LogRecord;
import java.util.logging.Level;


public class PSOProgramService extends Service implements Logger.ILoggerCallback
{
	private final String TAG = this.getClass().getSimpleName();

	static final int MSG_LINKPROGRAM			= 1; // incoming request message ID
	static final int MSG_LINKPROGRAM_SHMEM 		= 2; // incoming request message ID, using shared memory.
	static final int MSG_LINKPROGRAM_RESPONSE	= 10; // message ID sent in reply to MSG_LINKPROGRAM

	static final String VS_Key = "v";
	static final String PS_Key = "p";
	static final String CS_Key = "c";
	static final String PSOData_Key = "pso";
	static final String PSOCacheData_Key = "psocache";
	static final String JobContext_Key = "jtx";
	static final String JobID_Key = "jid";
	static final String ServiceID_Key = "sid";
	static final String CompiledProgram_Key = "cpg";
	static final String SHMem_Key = "shm";
	static final String JobFail = "f";
	static final String RobustContextKey = "rbc";
	public static final String LogDir = "/oglservice/";
	public static final String LogExt = ".txt";
	public static final Level LogLevel = Level.WARNING;

	public static final boolean bEnableTestFailures = false;
	public static final boolean bReportMemUse = false;
	public boolean bWantRobustContext = false;
	public boolean bGFXInitialized = false;
	private FileHandler logFileHandler;

	private void PrepareLogger() throws IOException
	{
		String name = getFilesDir().getAbsolutePath() + LogDir;
		File logDir = new File(name);
		logDir.mkdirs();
		name += TAG+"_%u_%g"+LogExt;
		Log.v(TAG, "log dir : "+name);
		logFileHandler = new FileHandler(name, 1024*32, 2, true);

		class BasicFormatter extends Formatter
		{
			private final Date date = new Date();
			private final int pid = android.os.Process.myPid();
			private final String SessionUID = UUID.randomUUID().toString();
			private long SequenceNum = 0;
			@Override
			public String format(final LogRecord record)
			{
				int tid = android.os.Process.myTid();
				date.setTime(record.getMillis());
				String Lines[] = record.getMessage().split(System.lineSeparator());
				StringBuilder returnmsg = new StringBuilder();
				for(String msg : Lines)
				{
					returnmsg.append(String.format("%s,%20d, %s %10d,%10d, %s %s\n", SessionUID, SequenceNum, date.toString(), pid, tid, record.getLevel().toString(), msg));
					SequenceNum++;
				}
				return returnmsg.toString();
			}
		}

		logFileHandler.setLevel(LogLevel);
		logFileHandler.setFormatter(new BasicFormatter());
	}

	public void LoggerCallback(String LevelIn, String Tag, String Message)
	{
		Level LogLevel = Level.INFO;
		switch(LevelIn)
		{
			case "V/":
			{
				LogLevel = Level.FINE;
				break;
			}
			case "D/":
			{
				LogLevel = Level.INFO;
				break;
			}
			case "W/":
			{
				LogLevel = Level.WARNING;
				break;
			}
			case "E/":
			{
				LogLevel = Level.SEVERE;
				break;
			}
		}
		logFileHandler.publish( new LogRecord(LogLevel, Message));
	}

	private final Logger logger = new Logger(this.getClass().getSimpleName());

	private class OGLUncaughtExceptionHandler implements Thread.UncaughtExceptionHandler
	{
		private final Lock uncaughtWait = new ReentrantLock();
		public void init()
		{
			Thread.setDefaultUncaughtExceptionHandler(this);
		}

		public void uncaughtException(Thread t, Throwable e)
		{
			// serialize exception processing..
			uncaughtWait.lock();
			try
			{
				StringBuilder stacktrace = new StringBuilder();
				for (StackTraceElement element : e.getStackTrace())
				{
					stacktrace.append(element.toString()).append("\n");
				}
				{
					Throwable causedby = e.getCause();
					while(causedby != null)
					{
						stacktrace.append("-~~ CAUSE ~~- (").append(causedby).append(")\n");
						for (StackTraceElement element : causedby.getStackTrace())
						{
							stacktrace.append(element.toString()).append("\n");
						}
						causedby = causedby.getCause();
					}
				}

				StringBuilder Message = new StringBuilder("Uncaught java exception!\n" + t + " thrown uncaught exception: " + e);
				{
					Throwable causedby = e.getCause();
					while(causedby != null)
					{
						Message.append("\nCause ").append(causedby).append("\n");
						causedby = causedby.getCause();
					}
				}
				logger.error(Message.toString());
				logger.error("Stack trace:");
				logger.error(stacktrace.toString());

				logFileHandler.flush();
				// kill ourselves and prevent OS from showing 'App has stopped' message.
				System.exit(0);
			}
			finally
			{
				uncaughtWait.unlock();
			}
		}
	}

	void LogMemInfo()
	{
		if( bReportMemUse == false)
		{
			return;
		}
		
		BufferedReader bufferedReader = null;
		try
		{
			File file = new File("/proc/self/status");
			bufferedReader = new BufferedReader(new FileReader(file));;
			String line;
			logger.verbose("---");
			while ((line = bufferedReader.readLine()) != null)
			{
				if(line.startsWith("Vm") || line.startsWith("Pid:") || line.startsWith("Rss"))
				{
					logger.verbose("psomeminfo: "+line);
				}
			}
			logger.verbose("---");
			bufferedReader.close();
		}
		catch (Exception e)
		{
			e.printStackTrace();
		}
		if (bufferedReader != null)
		{
			try
			{
				bufferedReader.close();
			}
			catch (Exception e) {
			}
		}
	}
	
	final OGLUncaughtExceptionHandler OGLUncaughtExceptionHandler = new OGLUncaughtExceptionHandler();

	static final boolean bEnableTrace = false;
	static void beginTrace(String msg)
	{
		if( bEnableTrace )
		{
			Trace.beginSection(msg);
		}
	}
	static void endTrace()
	{
		if( bEnableTrace )
		{
			Trace.endSection();
		}
	}

	public PSOProgramService()
	{
		OGLUncaughtExceptionHandler.init();
	}

	public boolean UseVulkan()
	{
		return false;
	}

	@Nullable
	@Override
	public IBinder onBind(Intent intent)
	{
		logger.debug("onBind "  + intent.toString());

		if(!bGFXInitialized)
		{
			if(!UseVulkan())
			{
				Bundle extras =intent.getExtras();
				if(extras != null)
				{
					bWantRobustContext = extras.getBoolean(RobustContextKey, false);
					logger.debug("robust "  + bWantRobustContext);
				}
				else
				{
					logger.debug("no robust set");
				}
				logger.verbose("initGLContext " );
				initGLContext();
			}
			else
			{
				logger.verbose("initVulkanContext " );
				initVulkanContext();
			}
			bGFXInitialized = true;
		}
		LogMemInfo();

		return mMessenger.getBinder();
	}

	final Messenger mMessenger = new Messenger(new IncomingHandler(Looper.getMainLooper())); // Target we publish for clients to send messages to IncomingHandler.

	/**
	 * Handler of incoming messages from client app.
	 */
	class IncomingHandler extends Handler {
		public IncomingHandler(Looper inLooper)
		{
			super(inLooper);
		}

		@Override
		public void handleMessage(Message msg) {
			switch (msg.what) {
				case MSG_LINKPROGRAM:
					beginTrace("PSOProgramService.handleMessage");
					if(UseVulkan())
					{
						byte[] VS = msg.getData().getByteArray(VS_Key);
						byte[] PS = msg.getData().getByteArray(PS_Key);
						byte[] PSOData = msg.getData().getByteArray(PSOData_Key);
						byte[] PSOCacheData = msg.getData().getByteArray(PSOCacheData_Key);

						int JobID = msg.getData().getInt(JobID_Key);
						int ServiceID = msg.getData().getInt(ServiceID_Key);

						byte[] JobContext = msg.getData().getByteArray(JobContext_Key);

						//logger.verbose("Processing program job "+JobID);
						ProcessVulkanProgramRequest(msg.replyTo, JobID, ServiceID, JobContext, VS, PS, PSOData, PSOCacheData);
					}
					else
					{
						String VS = msg.getData().getString(VS_Key);
						String PS = msg.getData().getString(PS_Key);
						String CS = msg.getData().getString(CS_Key);
						int JobID = msg.getData().getInt(JobID_Key);
						int ServiceID = msg.getData().getInt(ServiceID_Key);

						byte[] JobContext = msg.getData().getByteArray(JobContext_Key);

						//logger.verbose("Processing program job "+JobID);
						ProcessGLProgramRequest(msg.replyTo, JobID, ServiceID, JobContext, VS, PS, CS);
					}
					endTrace();

					break;
				case MSG_LINKPROGRAM_SHMEM:
					beginTrace("PSOProgramService.handleMessage MSG_LINKPROGRAM_SHMEM");
					if(UseVulkan())
					{
						ParcelFileDescriptor SharedFD 	= msg.getData().getParcelable(SHMem_Key);
						long VSSize						= msg.getData().getLong(VS_Key);
						long PSSize						= msg.getData().getLong(PS_Key);
						long PSODataSize				= msg.getData().getLong(PSOData_Key);
						long PSOCacheDataSize			= msg.getData().getLong(PSOCacheData_Key);
						int JobID						= msg.getData().getInt(JobID_Key);
						int ServiceID					= msg.getData().getInt(ServiceID_Key);
						byte[] JobContext				= msg.getData().getByteArray(JobContext_Key);
						int SHMemFD						= SharedFD.getFd();

						//logger.verbose("Processing program job "+JobID);
						ProcessVulkanProgramRequestSHM(msg.replyTo, JobID, ServiceID, JobContext, SHMemFD, VSSize, PSSize, PSODataSize, PSOCacheDataSize);
						try
						{
							SharedFD.close();
						}
						catch (IOException e) 
						{
							logger.error("error closing SH mem "+e.getMessage());
						}
					}
					else
					{
						logger.error("GLES Link program via SH mem is not yet supported");
					}
					endTrace();
				default:
					super.handleMessage(msg);
			}
		}
	}

	public void ProcessVulkanProgramRequest(Messenger replyTo, int JobID, int ServiceID, byte[] JobContext, byte[] VS, byte[] PS, byte[] PSOData, byte[] PSOCacheData)
	{
		ByteBuffer Result = null;
		try
		{
			byte[] PipelineCache = compileVulkanPSO(JobID, VS, PS, PSOData, PSOCacheData);
			Result = ByteBuffer.allocate(PipelineCache.length);
			Result.order(ByteOrder.nativeOrder());
			Result.put(PipelineCache);

			//logger.verbose(JobID+" CompileAndLink()  "+ (Result == null ? "null res " : Result.toString()));

		} catch (Exception e)
		{
			SendFail(replyTo, ServiceID, JobID, JobContext, e.getMessage());
		}

		if(Result != null)
		{
			//logger.verbose(JobID+" SendSuccess()  ");
			SendSuccess(replyTo, ServiceID, JobID, JobContext, Result.array());
		}
	}

	public void ProcessVulkanProgramRequestSHM(Messenger replyTo, int JobID, int ServiceID, byte[] JobContext, int SHMemFD, long VSSize, long PSSize, long PSODataSize, long PSOCacheDataSize)
	{
		int SHMResultFD = -1;
		try
		{
			// native code returns a shared FD which must be adopted in java land.
			SHMResultFD = compileVulkanPSOSHM(JobID, SHMemFD, VSSize, PSSize, PSODataSize, PSOCacheDataSize);
		}
		catch (Exception e)
		{
			SendFail(replyTo, ServiceID, JobID, JobContext, e.getMessage());
		}

		if(SHMResultFD != -1)
		{
			//logger.verbose(JobID+" SendSuccess()  ");
			// SendSuccess will adopt the shared FD.
			SendSuccess(replyTo, ServiceID, JobID, JobContext, SHMResultFD );
		}
		else
		{
			SendFail(replyTo, ServiceID, JobID, JobContext, "Failed to alloc bytes for PSO reply.");
		}
		LogMemInfo();
	}


	public void ProcessGLProgramRequest(Messenger replyTo, int JobID, int ServiceID, byte[] JobContext, String VS, String PS, String CS)
	{
		ByteBuffer Result = null;
		try
		{
			if( bEnableTestFailures && JobID == 500)
			{
				int splitpoint = VS.length()/2;
				String PartA = VS.substring(0, splitpoint);
				String PartC = VS.substring(splitpoint);
				VS = PartA + " 500 == SURPRISE! "+PartC;
			}
			Result = CompileAndLink(JobID, VS, PS, CS);
			//logger.verbose(JobID+" CompileAndLink()  "+ (Result == null ? "null res " : Result.toString()));

		} catch (Exception e)
		{
			SendFail(replyTo, ServiceID, JobID, JobContext, e.getMessage());
		}

		if(Result != null)
		{
			//logger.verbose(JobID+" SendSuccess()  ");
			SendSuccess(replyTo, ServiceID, JobID, JobContext, Result.array());
		}
	}

	@Override
	public void onCreate() {
		super.onCreate();
		try
		{
			android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);
			PrepareLogger();
		}
		catch (IOException e)
		{
			e.printStackTrace();
		}

		Logger.RegisterCallback(this);
		logger.verbose("oncreate " );
	}

	@Override
	public void onDestroy()
	{
		logger.verbose("onDestroy" );
		if(!UseVulkan())
		{
			destroyGLContext();
		}
		else
		{
			shutdownVulkanContext();
		}
		super.onDestroy();
	}

	public void SendSuccess(Messenger replyTo, int ServiceID, int JobID, byte[] context, byte[] compiledbinary)
	{
		SendSuccess(replyTo, ServiceID, JobID, context, compiledbinary, -1);
	}

	public void SendSuccess(Messenger replyTo, int ServiceID, int JobID, byte[] context, int ShmMemCompiledFD)
	{
		SendSuccess(replyTo, ServiceID, JobID, context, null, ShmMemCompiledFD);
	}

	public void SendSuccess(Messenger replyTo, int ServiceID, int JobID, byte[] context, byte[] compiledbinary, int ShmMemCompiledFD)
	{
		if( bEnableTestFailures && JobID == 1000 )
		{
			try
			{
				TimeUnit.SECONDS.sleep(11);
			} catch (InterruptedException e)
			{
				e.printStackTrace();
			}
			throw new RuntimeException("1000!");
		}
		beginTrace("PSOProgramService.SendSuccess");
		try
		{
			Message msg = Message.obtain(null, com.epicgames.unreal.psoservices.PSOProgramService.MSG_LINKPROGRAM_RESPONSE, 0, 0);
			Bundle params = new Bundle();

			params.putByteArray(JobContext_Key, context);
			if(compiledbinary != null)
			{
				params.putByteArray(CompiledProgram_Key, compiledbinary);
			}

			ParcelFileDescriptor parcelFD = null;

			if(ShmMemCompiledFD != -1)
			{
				// the FD is discarded after the message is sent.
				parcelFD = ParcelFileDescriptor.adoptFd(ShmMemCompiledFD);
				params.putParcelable(SHMem_Key, parcelFD);
			}
			params.putInt(JobID_Key, JobID);
			params.putInt(ServiceID_Key, ServiceID);
			msg.setData(params);

			replyTo.send(msg);
			if(parcelFD != null)
			{
				parcelFD.close();
			}
		}
		catch (Exception e)
		{
			logger.error(JobID+" SendSuccess(), failed to send reply : " + e);
			e.printStackTrace();
		}
		//logger.verbose(JobID+" SendSuccess() DONE ");

		endTrace();
	}

	public void SendFail(Messenger replyTo, int ServiceID, int JobID, byte[] context, String FailMessage)
	{
		beginTrace("PSOProgramService.SendFail");
		Message msg = Message.obtain(null, PSOProgramService.MSG_LINKPROGRAM_RESPONSE, 0, 0);
		Bundle params = new Bundle();
		params.putByteArray(JobContext_Key, context);
		params.putInt(JobID_Key, JobID);
		params.putInt(ServiceID_Key, ServiceID);
		params.putString(JobFail, FailMessage);
		msg.setData(params);
		try
		{
			replyTo.send(msg);
		}
		catch (RemoteException e)
		{
			logger.error(JobID+" SendFail(), failed to send reply "+FailMessage+" : " + e);
		}

		endTrace();
	}


	////////////////////////////////////////////// Vulkan context stuff
	public native void InitVKDevice();
	public native void ShutdownVKDevice();
	public native byte[] CompileVKGFXPSO(byte[] VertexShaderSource, byte[]  PixelShaderSource, byte[] PSOData, byte[] PSOCacheData);
	public native int CompileVKGFXPSOSHM(int SHMemFD, long VSSize, long PSSize, long PSODataSize, long PSOCacheDataSize);

	private void initVulkanContext()
	{
		beginTrace("PSOProgramService.InitVulkanContext");
		System.loadLibrary("psoservice");

		InitVKDevice();
		endTrace();
	}

	private byte[] compileVulkanPSO(int JobID, byte [] VertexShaderSource, byte []  PixelShaderSource, byte [] PSOData, byte [] PSOCacheData)
	{
		return CompileVKGFXPSO(VertexShaderSource, PixelShaderSource, PSOData, PSOCacheData);
	}

	private int compileVulkanPSOSHM(int JobID, int SHMemFD, long VSSize, long PSSize, long PSODataSize, long PSOCacheDataSize)
	{
		return CompileVKGFXPSOSHM(SHMemFD, VSSize, PSSize, PSODataSize, PSOCacheDataSize);
	}

	private void shutdownVulkanContext()
	{
		ShutdownVKDevice();
	}

	////////////////////////////////////////////// GL context stuff

	private EGLDisplay mEglDisplay = EGL14.EGL_NO_DISPLAY;
	private EGLContext mEglContext = EGL14.EGL_NO_CONTEXT;
	private EGLSurface mEglSurface = EGL14.EGL_NO_SURFACE;

	private void initGLContext()
	{
		beginTrace("PSOProgramService.InitContext");
		mEglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
		//logger.verbose("2 mEglDisplay "+mEglDisplay.toString()+" err "+EGL14.eglGetError());
		int[] version = new int[2];
		boolean eglInitialized = EGL14.eglInitialize(mEglDisplay, version,0, version, 1);
		if(!eglInitialized)
		{
			logger.error("eglInitialize "+eglInitialized+" err "+EGL14.eglGetError()+" ver: "+version[0]+"."+version[1]);
		}

		EGLConfig[] configs = new EGLConfig[1];
		int[] num_config = new int[1];
		int[] configSpec = new int[]
			{
				EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
				EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
				EGL14.EGL_BLUE_SIZE, 8,
				EGL14.EGL_RED_SIZE, 8,
				EGL14.EGL_GREEN_SIZE, 8,
				EGL14.EGL_DEPTH_SIZE, 8,
				EGL14.EGL_NONE
			};
		EGL14.eglChooseConfig(mEglDisplay, configSpec, 0, configs, 0, 1, num_config, 0);
		//logger.verbose("2 eglChooseConfig "+num_config[0]+" err "+EGL14.eglGetError()+" configs: "+configs[0].toString());
		int[] contextAttribsES32 = new int[]
			{
				EGLExt.EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
				EGLExt.EGL_CONTEXT_MINOR_VERSION_KHR, 2,
				EGL14.EGL_NONE
			};

		int EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT = 0x30BF;
		int[] contextAttribsES32Robust = new int[]
			{
				EGLExt.EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
				EGLExt.EGL_CONTEXT_MINOR_VERSION_KHR, 2,
				EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, 1,
				EGL14.EGL_NONE
			};

		String eglExtensions = EGL14.eglQueryString(mEglDisplay, EGL14.EGL_EXTENSIONS);
		boolean bUseRobustContext = bWantRobustContext && eglExtensions.contains("EGL_EXT_create_context_robustness");
		logger.verbose("2 eglCreateContext rbst "+bWantRobustContext+" ext contains "+eglExtensions.contains("EGL_EXT_create_context_robustness") );

		mEglContext = EGL14.eglCreateContext(mEglDisplay, configs[0], EGL14.EGL_NO_CONTEXT, bUseRobustContext ? contextAttribsES32Robust : contextAttribsES32, 0);
		logger.verbose("2 eglCreateContext "+mEglContext+", robust "+bUseRobustContext+", err "+EGL14.eglGetError());

		if (EGL14.eglQueryString(mEglDisplay, EGL14.EGL_EXTENSIONS).contains("EGL_KHR_surfaceless_context"))
		{
			mEglSurface = EGL14.EGL_NO_SURFACE;
			//logger.verbose("2 EGL_KHR_surfaceless_context ");
		}
		else
		{
			int[] pbufferAttribs = new int[]
				{
					EGL14.EGL_NONE
				};
			mEglSurface = EGL14.eglCreatePbufferSurface(mEglDisplay, configs[0], pbufferAttribs, 0);
			//logger.verbose("2 eglCreatePbufferSurface "+mEglSurface.toString());
		}
		boolean bSuccess = EGL14.eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext);
		if( !bSuccess )
		{
			logger.error("egl makecurrent failed. "+EGL14.eglGetError());
		}
		//logger.verbose("egldone.");
		endTrace();
	}

	private void destroyGLContext()
	{
		if( mEglSurface != EGL14.EGL_NO_SURFACE)
		{
			EGL14.eglDestroySurface(mEglDisplay, mEglSurface);
			mEglSurface = EGL14.EGL_NO_SURFACE;
		}
		if( mEglContext != EGL14.EGL_NO_CONTEXT)
		{
			EGL14.eglDestroyContext(mEglDisplay, mEglContext);
			mEglContext = EGL14.EGL_NO_CONTEXT;
		}
		mEglDisplay = EGL14.EGL_NO_DISPLAY;
	}

	static class GLCompileException extends Exception
	{
		public GLCompileException(String message)
		{
			super(message);
		}
	}

	private int createShader(int shaderType, String source) throws GLCompileException
	{
		int shader = GLES31.glCreateShader(shaderType);
		if( shader == 0 ) {
			throw new GLCompileException("Failed to create shader type: " + shaderType);
		}
		beginTrace("PSOProgramService.createShader "+shaderType);
		GLES31.glShaderSource(shader, source);
		//logger.debug("2 createShader err "+GLES31.glGetError());
		GLES31.glCompileShader(shader);
		endTrace();
		//logger.debug("2 glCompileShader err "+GLES31.glGetError());

		int[] compiled = new int[1];
		GLES31.glGetShaderiv(shader, GLES31.GL_COMPILE_STATUS, compiled, 0);
		//logger.debug("2 glGetShaderiv, compiled="+compiled[0]+" err "+GLES31.glGetError());
		if (compiled[0] == 0)
		{
			String ShaderFailLog = GLES31.glGetShaderInfoLog(shader);
			GLES31.glDeleteShader(shader);
			String ShaderTypeName = String.valueOf(shaderType);
			switch(shaderType)
			{
				case GLES31.GL_VERTEX_SHADER:
				{
					ShaderTypeName = "vertex shader";
					break;
				}
				case GLES31.GL_FRAGMENT_SHADER:
				{
					ShaderTypeName = "pixel shader";
					break;
				}
				case GLES31.GL_COMPUTE_SHADER:
				{
					ShaderTypeName = "compute shader";
					break;
				}					
			}
			throw new GLCompileException("Failed to compile shader type: " + ShaderTypeName+" log: "+ShaderFailLog);
		}
		return shader;
	}

	private ByteBuffer CompileAndLink(int JobID, String VertexShaderSource, String PixelShaderSource, String ComputeShaderSource)  throws GLCompileException
	{
		java.nio.ByteBuffer RetBuffer = null;
		int mPixelShaderID = 0;
		int mVertexShaderID = 0;
		int mComputeShaderID = 0;
		int mProgram = 0;

		try {
			beginTrace("PSOProgramService.CompileAndLink "+JobID);

			mProgram = GLES31.glCreateProgram();
			if (mProgram <= 0) {
				throw new GLCompileException("Failed to create Program");
			}
			GLES31.glProgramParameteri(mProgram, GLES31.GL_PROGRAM_BINARY_RETRIEVABLE_HINT, 1);

			if( ComputeShaderSource == null)
			{
				mVertexShaderID = createShader(GLES31.GL_VERTEX_SHADER, VertexShaderSource);
				mPixelShaderID = createShader(GLES31.GL_FRAGMENT_SHADER, PixelShaderSource);

				GLES31.glAttachShader(mProgram, mVertexShaderID);
				GLES31.glAttachShader(mProgram, mPixelShaderID);
			}
			else
			{
				mComputeShaderID = createShader(GLES31.GL_COMPUTE_SHADER, ComputeShaderSource);
				GLES31.glAttachShader(mProgram, mComputeShaderID);
				//logger.debug(JobID+" glAttachShader(cs) err "+GLES31.glGetError());
			}

			beginTrace("PSOProgramService.CompileAndLink LINKING"+JobID);
			GLES31.glLinkProgram(mProgram);
			endTrace();

			int LinkStatus[] = new int[]{0};
			GLES31.glGetProgramiv(mProgram, GLES31.GL_LINK_STATUS, LinkStatus, 0);
			if( LinkStatus[0] != GLES31.GL_TRUE)
			{
				String ProgramLinkFailLog = GLES31.glGetProgramInfoLog(mProgram);
				throw new GLCompileException("Failed to link program: " + ProgramLinkFailLog);
			}

			//logger.debug(JobID+" glLinkProgram() err "+GLES31.glGetError());
			// pull binary from linked program
			int BinaryLength[] = new int[]{0};
			GLES31.glGetProgramiv(mProgram, GLES31.GL_PROGRAM_BINARY_LENGTH, BinaryLength, 0);
			//logger.debug(JobID+" glGetProgramiv() BinaryLength="+BinaryLength[0]+" err "+GLES31.glGetError());
			if (BinaryLength[0] > 0)
			{
				int BinaryLengthOUT[] = new int[]{0};
				int BinaryFormatOUT[] = new int[]{0};
				java.nio.ByteBuffer Buffer = java.nio.ByteBuffer.allocateDirect(BinaryLength[0]);
				GLES31.glGetProgramBinary(mProgram, BinaryLength[0], BinaryLengthOUT, 0, BinaryFormatOUT, 0, Buffer);

				RetBuffer = ByteBuffer.allocate(4 + BinaryLengthOUT[0]);
				RetBuffer.order(ByteOrder.nativeOrder());
				RetBuffer.putInt(BinaryFormatOUT[0]);
				RetBuffer.put(Buffer);

				//logger.verbose(JobID+" glGetProgramBinary() BinaryLength="+BinaryLengthOUT[0]+" BinaryFormatOUT "+BinaryFormatOUT[0]+"err "+GLES31.glGetError());
			}
		}
		finally
		{
			endTrace();
			if(mVertexShaderID != 0)
			{
				GLES31.glDetachShader(mProgram, mVertexShaderID);
				GLES31.glDeleteShader(mVertexShaderID);
			}
			if(mPixelShaderID != 0)
			{
				GLES31.glDetachShader(mProgram, mPixelShaderID);
				GLES31.glDeleteShader(mPixelShaderID);
			}

			if(mComputeShaderID != 0)
			{
				GLES31.glDetachShader(mProgram, mComputeShaderID);
				GLES31.glDeleteShader(mComputeShaderID);
			}

			if(mProgram != 0)
			{
				GLES31.glDeleteProgram(mProgram);
			}

			if(RetBuffer == null)
			{
				logger.error(JobID+" glGetProgramBinary() err "+GLES31.glGetError());
			}
		}
		return RetBuffer;
	}
}

