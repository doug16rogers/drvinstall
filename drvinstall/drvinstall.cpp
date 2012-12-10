// drvinstall.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <iostream>
#include <string>

using namespace std;

#ifdef UNICODE

#define M_T(_s)  L ## _s
#define tstring  wstring
#define tchar_t  wchar_t
#define tcin     wcin
#define tcout    wcout
#define tcerr    wcerr
#define tclog    wclog
typedef wchar_t char_t;

inline std::string to_tstring(const tstring& tstr)
{
	std::string result;

	for (tstring::const_iterator it = tstr.begin(); it < tstr.end(); ++it)
	{
		result.push_back((char) *it);
	}

	return result;
}

#else

#define M_T(_s)  _s
#define tstring  string
#define tchar_t  char
#define tcin     cin
#define tcout    cout
#define tcerr    cerr
#define tclog    clog
#define to_string(_tstr)  (_tstr)

#endif

/**
 * Name of this program.
 */
#define PROGRAM  M_T("drvinstall")

#if 0
class service_handle
{
private:
	SC_HANDLE handle_;
	explicit service_handle(const service_handle&);

public:
	class error : public std::exception
	{
		explicit error(const error&);
		error();
	public:
		error(const tstring& message) : std::exception(to_tstring(message).c_str()) { }
	};

	explicit service_handle();

	SC_HANDLE operator()() const
	{
		return handle_;
	}

	SC_HANDLE open_sc_manager(DWORD access_rights_)
	{
		if (NULL != handle_)
			throw error(M_T("handle already open"));

		handle_ = OpenSCManager(NULL, NULL, access_rights_);
		return handle_;
	}

	SC_HANDLE create_service(const tstring& driver_path,
							 const tstring& service_name,
							 const tstring& service_description)
	{
	}

	~service_handle()
	{
		if (handle_) ::CloseServiceHandle(handle_);
	}
};   // class service_handle
#endif

// ----------------------------------------------------------------------------
/**
 * @param error - a value returned by GetLastError().
 *
 * @return a string representation of the error.
 */
tstring error_message(DWORD error_number)
{
	tstring result = M_T("uknown");
	LPTSTR heap_message = NULL;
	DWORD format_result = ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
										  FORMAT_MESSAGE_FROM_SYSTEM |
										  FORMAT_MESSAGE_ARGUMENT_ARRAY,
										  0, error_number, LANG_NEUTRAL, (LPTSTR) &heap_message, 0, 0);

	if (0 != format_result)
	{
		result = heap_message;
	}

	if (heap_message)
	{
		LocalFree(HLOCAL(heap_message));
	}

	return result;
}   // error_message()

// ----------------------------------------------------------------------------
/**
 * Installs a device driver.
 *
 * @param driver_path - path to driver file.
 *
 * @param service_name - name of service.
 *
 * @param service_description - description of service; will be assigned to
 * to corresponding registry key.
 *
 * @return a handle to the running service, or NULL on error.
 */
bool install_driver(
	const wstring& driver_path, 
	const wstring& service_name,
	const wstring& service_description)
{
	bool result = false;
	const DWORD access_rights = SC_MANAGER_ALL_ACCESS;
	SC_HANDLE service = NULL;

	tcout << PROGRAM << ": calling OpenSCManager()." << endl;
	SC_HANDLE service_manager = OpenSCManager(NULL, NULL, access_rights);

	if (NULL != service_manager)
	{
		DWORD tag_id = 0;
		tcout << PROGRAM << ": calling CreateService()." << endl;
		service = ::CreateService(service_manager,
								  service_name.c_str(),   // Service name.
								  service_name.c_str(),   // Display name for service.
								  SERVICE_ALL_ACCESS,     // All access.
								  SERVICE_KERNEL_DRIVER,
								  SERVICE_DEMAND_START,
								  SERVICE_ERROR_NORMAL,
								  driver_path.c_str(),    // Path to service binary.
								  NULL,
								  NULL, //&tag_id,
								  NULL,
								  NULL,
								  NULL);

		const DWORD last_error = ::GetLastError();

		if (NULL == service)
		{
			tcerr << PROGRAM << M_T("CreateService() returned NULL; last error=") << last_error
				  << M_T(": ") << error_message(last_error)
				  << M_T(".") << endl;
		}
		else
		{
			result = true;
			CloseServiceHandle(service);
			tcout << PROGRAM << M_T(": tag_id=") << tag_id << endl;
		}

		CloseServiceHandle(service_manager);
	}   // If service manager opened successfully.

	// Set service description in registry.

	if (service && !service_description.empty())
	{
		tstring key_name = M_T("SYSTEM\\CurrentControlSet\\Services\\");
		key_name.append(service_name);
		HKEY hkey = NULL;

		tcout << PROGRAM << M_T(": calling RegOpenKeyEx(\"") << key_name << M_T("\").") << endl;
		if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, key_name.c_str(), 0, KEY_WRITE|KEY_READ, &hkey))
		{
			const size_t description_bytes = (service_description.size() + 1) * sizeof(tchar_t);

			tcout << PROGRAM << M_T(": calling RegSetValueEx(\"Description\", \"")
				  << service_description.c_str() << M_T("\").") << endl;
			if (ERROR_SUCCESS != ::RegSetValueEx(hkey, M_T("Description"), NULL, REG_SZ, 
												 reinterpret_cast<const BYTE *>(service_description.c_str()),
												 static_cast<DWORD>(description_bytes)))
			{
				tcerr << PROGRAM << M_T("could not set Description in registry.") << endl;
			}
		}
	}   // If service was created.

	return result;
}   // install_driver()

// ----------------------------------------------------------------------------
/**
 * Uninstalls the device driver with the given @a service_name.
 *
 * @param service_name - name of service.
 *
 * @return true on success, false otherwise.
 */
bool uninstall_driver(const wstring& service_name)
{
	bool result = false;
	const DWORD access_rights = SC_MANAGER_ALL_ACCESS;
	SC_HANDLE service = NULL;
	SC_HANDLE service_manager = OpenSCManager(NULL, NULL, access_rights);
	DWORD last_error = ::GetLastError();

	if (service_manager)
	{
		service = ::OpenService(service_manager, service_name.c_str(), DELETE);
		last_error = ::GetLastError();

		if (service)
		{
			// ::StopService(service);
			::DeleteService(service);
			last_error = ::GetLastError();
			::CloseServiceHandle(service);
		}

		::CloseServiceHandle(service_manager);
	}   // if service_manager

	return (ERROR_SUCCESS == last_error);
}   // uninstall_driver()

// ----------------------------------------------------------------------------
/**
 * Main program.
 */
int _tmain(int argc, _TCHAR* argv[])
{
	bool uninstall = false;
	bool test_only = false;

	// Look for -u.

	if ((argc > 1) && (tstring(argv[1]) == M_T("-u")))
	{
		uninstall = true;

		// Remove that argument.
		for (int i = 2; i < argc; i++)
		{
			argv[i-1] = argv[i];
		}

		--argc;
	}

	if ((argc > 1) && (tstring(argv[1]) == M_T("-T")))
	{
		test_only = true;

		// Remove that argument.
		for (int i = 2; i < argc; i++)
		{
			argv[i-1] = argv[i];
		}

		--argc;
	}

	if (argc < 2)
	{
		cerr << "Usage: " << PROGRAM << " [-u] sys-file [\"service-description\"]" << endl;
		return 1;
	}

	tstring driver_file(argv[1]);
	size_t slash_index = driver_file.find_last_of(M_T("/\\"));

	if (slash_index == tstring::npos)
	{
		slash_index = 0;
	}
	else
	{
		++slash_index;
	}

	size_t dot_index = driver_file.find(M_T('.'), slash_index);

	if (dot_index == tstring::npos)
	{
		wcerr << PROGRAM << M_T(": cannot extract driver service name - no dot in \"")
			  << driver_file << M_T("\".") << endl;
		return 1;
	}

	wstring service_name = driver_file.substr(slash_index, dot_index - slash_index);

	if (service_name.empty())
	{
		wcerr << PROGRAM << M_T(": driver file cannot begin with dot (\"") << driver_file << M_T("\".") << endl;
		return 1;
	}

	wstring service_description = service_name + M_T(" driver service");

	if (argc > 2)
	{
		service_description = argv[2];
	}

	try
	{
		if (uninstall)
		{
			wcout << PROGRAM << M_T(": uninstalling \"")
				  << driver_file << M_T("\" with service name '")
				  << service_name << M_T("'.") << endl;

			if (test_only)
			{
				wcout << PROGRAM << M_T(": skipping uninstall due to -T.") << endl;
			}
			else
			{
				uninstall_driver(service_name);
			}
		}
		else
		{
			wcout << PROGRAM << M_T(": installing \"")
				  << driver_file << M_T("\" with service name '")
				  << service_name << M_T("', \"") << service_description << M_T("\".") << endl;

			if (test_only)
			{
				wcout << PROGRAM << M_T(": skipping uninstall due to -T.") << endl;
			}
			else
			{
				install_driver(driver_file, service_name, service_description);
			}
		}   // else installing
	}   // try
	catch (const exception& exc)
	{
		cerr << exc.what() << endl;
		return 2;
	}

	return 0;
}   // _tmain()
