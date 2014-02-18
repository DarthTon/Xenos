#pragma once

#include "stdafx.h"
#include "resource.h"
#include "DlgModules.h"
#include "ConfigMgr.h"
#include "../../BlackBone/src/BlackBone/Process.h"
#include "../../BlackBone/src/BlackBone/FileProjection.h"
#include "../../BlackBone/src/BlackBone/PEParser.h"
#include "../../BlackBone/src/BlackBone/Utils.h"

class MainDlg
{
    typedef INT_PTR ( MainDlg::*PDLGPROC)(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

    typedef std::map<UINT, PDLGPROC> mapMsgProc;
    typedef std::map<WORD, PDLGPROC> mapCtrlProc;

    enum MapMode
    {
        Normal = 0,
        Manual = 1
    };

public:
	
	~MainDlg();

    static MainDlg& Instance();

    INT_PTR Run();

	static INT_PTR CALLBACK DlgProcMain(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

private:
    MainDlg();
    MainDlg( MainDlg& root );
    MainDlg& operator=(MainDlg&);

private:
    DWORD AttachToProcess(DWORD pid);
    DWORD FillProcessList();
    DWORD FillThreads();
    DWORD SetActiveProcess( bool createNew, const wchar_t* path, DWORD pid = 0xFFFFFFFF );
    DWORD MmapFlags();
    DWORD MmapFlags( DWORD flags );
    DWORD SetMapMode( MapMode mode );

    DWORD LoadImageFile( const wchar_t* path );
    DWORD ValidateImage( const wchar_t* path, const char* init );
    DWORD ValidateArch( const wchar_t* path, const blackbone::Wow64Barrier& barrier, DWORD thdId, bool isManual );
    DWORD ValidateInit( const char* init );

    DWORD DoInject( const wchar_t* path, const char* init, const wchar_t* arg );
    static DWORD CALLBACK InjectWrap( LPVOID lpPram );
    DWORD InjectWorker( std::wstring path, std::string init, std::wstring arg );

//////////////////////////////////////////////////////////////////////////////////
    INT_PTR OnInit(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);      //
    INT_PTR OnCommand(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);   //
    INT_PTR OnClose(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);     //
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
    INT_PTR OnFileExit(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);  //
    INT_PTR OnLoadImage( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnExecute(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR OnDropDown(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR OnSelChange(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR OnDragDrop( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnNewProcess( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnEjectModules( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    //////////////////////////////////////////////////////////////////////////////
private:
    ModulesDlg          _modulesDlg;
	HWND                _hMainDlg;
    static mapMsgProc   Messages;
    mapCtrlProc         Events;
    blackbone::Process  _proc;
    ConfigMgr           _config;

    blackbone::FileProjection _file;
    blackbone::pe::PEParser   _imagePE;
    std::wstring              _procPath;
    bool                      _newProcess;

    std::wstring _path;
    std::string  _init;
    std::wstring _arg;

};