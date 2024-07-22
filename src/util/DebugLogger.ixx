module;
#include <guiddef.h>

#include <Windows.h>
#include <shlobj.h>
#include <Shlwapi.h>
#include <inspectable.h>



#ifdef ENABLE_SENTRY
#define SENTRY_BUILD_STATIC 1
#include "../lib/sentry/include/sentry.h"
#endif


export module DebugLogger;


import std;
import std.compat;
import Encoding;


export enum class DebugInterestLevel {
    NotInterested,
    Normal,
    Interested
};

export using LookupInfoT = std::pair<std::string_view, DebugInterestLevel>;


export class DebugLogger
{

public:
    // std::source_location::current();
    static void OnQueryInterfaceEntry(const GUID& riid, const std::source_location location, const char* funcName);
    static void OnQueryInterfaceExitUnhandled(const GUID& riid, const std::source_location location, const char* funcName);
    static void TraceLog(const std::string& message, const std::source_location location, const char* funcName);
    static void TraceLog(const std::wstring& message, const std::source_location location, const char* funcName);
    static void WarnLog(const std::string& message, const std::source_location location, const char* funcName);

    static void CaptureException(std::exception& exception, std::map<std::string, std::string> attributes = {});

    static bool IsIIDUninteresting(const GUID& riid);
    static LookupInfoT GetGUIDName(const GUID& guid);
};


using LookupInfoStorageT = std::pair<std::string, DebugInterestLevel>;

static std::pair<GUID, LookupInfoStorageT> LookupFromText(const char* name, const wchar_t* guid, DebugInterestLevel level = DebugInterestLevel::Normal) {
    GUID res;

    CLSIDFromString(guid, &res);
    return { res, { name, level } };
}

template<typename T>
concept IsIUnknown = std::is_base_of_v<IUnknown, T>;


template<IsIUnknown T>
constexpr std::pair<GUID, LookupInfoStorageT> LookupFromType(DebugInterestLevel level = DebugInterestLevel::Normal) {

    std::string_view name(typeid(T).name());


    if (name.starts_with("struct"))
        name = name.substr(7);
    if (name.starts_with("class"))
        name = name.substr(6);

    return { __uuidof(T), { std::format("IID_{}", name), level } };
}


namespace std {
    template <>
    struct hash<GUID>
    {
        std::size_t operator()(const GUID& k) const
        {
            return hash<std::string_view>()(std::string_view((char*)&k, sizeof(GUID)));
        }
    };
}



import GUIDLookup;

static GUIDLookup<LookupInfoStorageT> guidLookupTable{

#pragma push_macro("DEFINE_GUID")
#undef DEFINE_GUID

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    {GUID{l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8}, {#name, DebugInterestLevel::Normal} },


#include <ShlGuid.h>

#undef DEFINE_GUID
    // generally not interested in these as they are all undocumented anyway
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    {GUID{l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8}, {#name, DebugInterestLevel::NotInterested} },


    DEFINE_GUID(IID_IObjectWithGITCookie, 0x527832F6, 0x5FB1, 0x414D, 0x86, 0xCC, 0x5B, 0xC1, 0xDA, 0x0F, 0x4E, 0xD9)
        DEFINE_GUID(IID_IShellFolder3, 0x711B2CFD, 0x93D1, 0x422b, 0xBD, 0xF4, 0x69, 0xBE, 0x92, 0x3F, 0x24, 0x49)
        DEFINE_GUID(IID_IFolderType, 0x053B4A86, 0x0DC9, 0x40A3, 0xB7, 0xED, 0xBC, 0x6A, 0x2E, 0x95, 0x1F, 0x48)
        DEFINE_GUID(IID_INavigationRelatedItem, 0x51737562, 0x4F12, 0x4B7A, 0xA3, 0xA0, 0x6E, 0x9E, 0xFE, 0x46, 0x5E, 0x1E)
        DEFINE_GUID(IID_IShellFolderViewCB, 0x2047E320, 0xF2A9, 0x11CE, 0xAE, 0x65, 0x08, 0x00, 0x2B, 0x2E, 0x12, 0x62)
        DEFINE_GUID(IID_ISearchBoxSettings, 0x9838AAB6, 0x32FD, 0x455A, 0x82, 0x3D, 0x83, 0xCF, 0xE0, 0x6E, 0x4D, 0x48)
        DEFINE_GUID(IID_IShellSearchTargetServices, 0xDDA3A58A, 0x43DA, 0x4A43, 0xA5, 0xF2, 0xF7, 0xAB, 0xF6, 0xE3, 0xC0, 0x26)
        DEFINE_GUID(IID_IInfoPaneProvider, 0x38698B65, 0x1CA7, 0x458C, 0xB4, 0xD6, 0xE0, 0xA5, 0x13, 0x79, 0xC1, 0xD2)
        DEFINE_GUID(IID_ICacheableObject, 0x771E5917, 0x5788, 0x4A36, 0xA2, 0x76, 0xB0, 0xDD, 0xBF, 0x8E, 0x4A, 0xBF)
        DEFINE_GUID(IID_IdentityUnmarshal, 0x0000001B, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)
        DEFINE_GUID(IID_IApplicationFrame, 0x143715D9, 0xA015, 0x40EA, 0xB6, 0x95, 0xD5, 0xCC, 0x26, 0x7E, 0x36, 0xEE)
        DEFINE_GUID(IID_IApplicationFrameManager, 0xD6DEFAB3, 0xDBB9, 0x4413, 0x8A, 0xF9, 0x55, 0x45, 0x86, 0xFD, 0xFF, 0x94)
        DEFINE_GUID(IID_IApplicationFrameEventHandler, 0xEA5D0DE4, 0x770D, 0x4DA0, 0xA9, 0xF8, 0xD7, 0xF9, 0xA1, 0x40, 0xFF, 0x79)
        DEFINE_GUID(IID_IStreamGroup, 0x816E5B3E, 0x5523, 0x4EFC, 0x92, 0x23, 0x98, 0xEC, 0x42, 0x14, 0xC3, 0xA0)
        DEFINE_GUID(IID_IPimcContext2, 0x1868091E, 0xAB5A, 0x415F, 0xA0, 0x2F, 0x5C, 0x4D, 0xD0, 0xCF, 0x90, 0x1D)
        DEFINE_GUID(IID_IEUserBroker, 0x1AC7516E, 0xE6BB, 0x4A69, 0xB6, 0x3F, 0xE8, 0x41, 0x90, 0x4D, 0xC5, 0xA6)
        DEFINE_GUID(IID_IFrameTaskManager, 0x35bd3360, 0x1b35, 0x4927, 0xBa, 0xe4, 0xb1, 0x0e, 0x70, 0xd9, 0x9e, 0xff)
        DEFINE_GUID(IID_IVerbStateTaskCallBack, 0xf2153260, 0x232e, 0x4474, 0x9d, 0x0a, 0x9f, 0x2a, 0xb1, 0x53, 0x44, 0x1d)
        DEFINE_GUID(IID_IShellFolderPropertyInformation, 0x124bae2c, 0xcb94, 0x42cd, 0xb5, 0xb8, 0x43, 0x58, 0x78, 0x96, 0x84, 0xef)
        DEFINE_GUID(IID_IContextMenuFactory, 0x47d9e2b2, 0xcbb3, 0x4fe3, 0xa9, 0x25, 0xf4, 0x99, 0x78, 0x68, 0x59, 0x82)

        DEFINE_GUID(IID_ITopViewAwareItem, 0x8279FEB8, 0x5CA4, 0x45C4, 0xBE, 0x27, 0x77, 0x0D, 0xCD, 0xEA, 0x1D, 0xEB)
        DEFINE_GUID(IID_IConnectionFactory, 0x6FE2B64C, 0x5012, 0x4B88, 0xBB, 0x9D, 0x7C, 0xE4, 0xF4, 0x5E, 0x37, 0x51)
        DEFINE_GUID(IID_IFrameLayoutDefinition, 0x176C11B1, 0x4302, 0x4164, 0x84, 0x30, 0xD5, 0xA9, 0xF0, 0xEE, 0xAC, 0xDB)
        DEFINE_GUID(IID_IItemSetOperations, 0x32AE3A1F, 0xD90E, 0x4417, 0x9D, 0xD9, 0x23, 0xB0, 0xDF, 0xA4, 0x62, 0x1D)


        // https://github.com/google/google-drive-shell-extension/blob/master/DriveFusion/GDriveShlExt.cpp#L86

        LookupFromText("IID_IFrameLayoutDefinitionFactory", L"{7E734121-F3B4-45F9-AD43-2FBE39E533E2}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_INewItemAdvisor", L"{24D16EE5-10F5-4DE3-8766-D23779BA7A6D}", DebugInterestLevel::NotInterested),

        LookupFromText("IID_Unknown_1", L"{93F81976-6A0D-42C3-94DD-AA258A155470}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_2", L"{CAD9AE9F-56E2-40F1-AFB6-3813E320DCFD}", DebugInterestLevel::NotInterested),


        // ObjIdl.h

        LookupFromType<IMarshal>(DebugInterestLevel::NotInterested),
        LookupFromType<INoMarshal>(DebugInterestLevel::NotInterested),
        LookupFromType<IAgileObject>(DebugInterestLevel::NotInterested),
        LookupFromType<IActivationFilter>(DebugInterestLevel::NotInterested),
        LookupFromType<IMalloc>(DebugInterestLevel::NotInterested),
        LookupFromType<IStdMarshalInfo>(DebugInterestLevel::NotInterested),
        LookupFromType<IExternalConnection>(DebugInterestLevel::NotInterested),
        LookupFromType<IMultiQI>(),
        LookupFromType<AsyncIMultiQI>(),
        LookupFromType<IInternalUnknown>(),
        LookupFromType<IEnumUnknown>(),
        LookupFromType<IEnumString>(),
        LookupFromType<ISequentialStream>(DebugInterestLevel::Interested),
        LookupFromType<IStream>(DebugInterestLevel::Interested),
        LookupFromType<IRpcChannelBuffer>(),
        LookupFromType<IAsyncRpcChannelBuffer>(),
        LookupFromType<IRpcSyntaxNegotiate>(),
        LookupFromType<IRpcProxyBuffer>(),
        LookupFromType<IRpcStubBuffer>(),
        LookupFromType<IPSFactoryBuffer>(),
        LookupFromType<IChannelHook>(),
        LookupFromType<IClientSecurity>(),
        LookupFromType<IServerSecurity>(),
        LookupFromType<IRpcOptions>(),
        LookupFromType<IGlobalOptions>(),
        LookupFromType<ISurrogate>(),
        LookupFromType<IGlobalInterfaceTable>(),
        LookupFromType<ISynchronize>(),
        LookupFromType<ISynchronizeHandle>(),
        LookupFromType<ISynchronizeEvent>(),
        LookupFromType<ISynchronizeContainer>(),
        LookupFromType<ISynchronizeMutex>(),
        LookupFromType<ICancelMethodCalls>(),
        LookupFromType<IAsyncManager>(),
        LookupFromType<ICallFactory>(DebugInterestLevel::NotInterested),
        LookupFromType<IRpcHelper>(),
        LookupFromType<IReleaseMarshalBuffers>(),
        LookupFromType<IWaitMultiple>(),
        LookupFromType<IAddrTrackingControl>(),
        LookupFromType<IAddrExclusionControl>(),
        LookupFromType<IPipeByte>(),
        LookupFromType<AsyncIPipeByte>(),
        LookupFromType<IPipeLong>(),
        LookupFromType<AsyncIPipeLong>(),
        LookupFromType<IPipeDouble>(),
        LookupFromType<AsyncIPipeDouble>(),
        LookupFromType<IComThreadingInfo>(),
        LookupFromType<IProcessInitControl>(),
        LookupFromType<IFastRundown>(),
        LookupFromType<IMarshalingStream>(),
        LookupFromType<IAgileReference>(),
        LookupFromType<IAgileReference>(),
        LookupFromType<IMallocSpy>(),
        LookupFromType<IBindCtx>(),
        LookupFromType<IEnumMoniker>(),
        LookupFromType<IRunnableObject>(),
        LookupFromType<IRunningObjectTable>(),
        LookupFromType<IPersist>(DebugInterestLevel::Interested),
        LookupFromType<IPersistStream>(DebugInterestLevel::Interested),
        LookupFromType<IMoniker>(),
        LookupFromType<IROTData>(),
        LookupFromType<IEnumSTATSTG>(),
        LookupFromType<IStorage>(DebugInterestLevel::Interested),
        LookupFromType<IPersistFile>(DebugInterestLevel::Interested),
        LookupFromType<IPersistStorage>(DebugInterestLevel::Interested),
        LookupFromType<ILockBytes>(),
        LookupFromType<IEnumFORMATETC>(),
        LookupFromType<IEnumSTATDATA>(),
        LookupFromType<IRootStorage>(),
        LookupFromType<IAdviseSink>(),
        LookupFromType<AsyncIAdviseSink>(),
        LookupFromType<IDataObject>(DebugInterestLevel::Interested),
        LookupFromType<IDataAdviseHolder>(),
        LookupFromType<IMessageFilter>(),
        LookupFromType<IClassActivator>(),
        LookupFromType<IFillLockBytes>(),
        LookupFromType<IProgressNotify>(DebugInterestLevel::Interested),
        LookupFromType<ILayoutStorage>(),
        LookupFromType<IBlockingLock>(),
        LookupFromType<ITimeAndNoticeControl>(),
        LookupFromType<IOplockStorage>(),
        LookupFromType<IDirectWriterLock>(),
        LookupFromType<IUrlMon>(),
        LookupFromType<IForegroundTransfer>(DebugInterestLevel::Interested),
        LookupFromType<IThumbnailExtractor>(DebugInterestLevel::Interested),
        LookupFromType<IDummyHICONIncluder>(),
        LookupFromType<IProcessLock>(),
        LookupFromType<ISurrogateService>(),
        LookupFromType<IInitializeSpy>(),
        LookupFromType<IApartmentShutdown>(),

        // oleidl.h
        LookupFromType<IOleAdviseHolder>(),
        LookupFromType<IOleCache>(),
        LookupFromType<IOleCacheControl>(),
        LookupFromType<IParseDisplayName>(),
        LookupFromType<IOleClientSite>(),
        LookupFromType<IOleObject>(),
        LookupFromType<IOleWindow>(),
        LookupFromType<IOleLink>(),
        LookupFromType<IContinue>(),
        LookupFromType<IViewObject>(),
        LookupFromType<IDropSource>(DebugInterestLevel::Interested),
        LookupFromType<IDropTarget>(DebugInterestLevel::Interested),
        LookupFromType<IDropSourceNotify>(),
        LookupFromType<IEnterpriseDropTarget>(),
        LookupFromType<IEnumOLEVERB>(),
        LookupFromType<IOleCommandTarget>(DebugInterestLevel::NotInterested),


        // ShObjIdl_core.h IUnknown
        LookupFromType<IContextMenu>(DebugInterestLevel::Interested), // Want to know if contextMenu is missing somewhere somewhere
        LookupFromType<IContextMenu2>(DebugInterestLevel::Normal), // 2/3 are just for menu messages which we don't use
        LookupFromType<IContextMenu3>(DebugInterestLevel::Normal),
        LookupFromType<IExecuteCommand>(),
        LookupFromType<IPersistFolder>(DebugInterestLevel::Interested),
        LookupFromType<IRunnableTask>(),
        LookupFromType<IShellTaskScheduler>(),
        LookupFromType<IPersistFolder2>(DebugInterestLevel::Interested),
        LookupFromType<IPersistFolder3>(DebugInterestLevel::Interested),
        LookupFromType<IPersistIDList>(),
        LookupFromType<IEnumIDList>(),
        LookupFromType<IEnumFullIDList>(),
        LookupFromType<IFileSyncMergeHandler>(),
        LookupFromType<IObjectWithFolderEnumMode>(),
        LookupFromType<IParseAndCreateItem>(),
        LookupFromType<IShellFolder>(DebugInterestLevel::Interested),
        LookupFromType<IEnumExtraSearch>(),
        LookupFromType<IShellFolder2>(DebugInterestLevel::Interested),
        LookupFromType<IShellView>(DebugInterestLevel::Interested),
        LookupFromType<IShellView2>(DebugInterestLevel::Interested),
        LookupFromType<IFolderView>(DebugInterestLevel::Interested),
        LookupFromType<IFolderView2>(DebugInterestLevel::Interested),
        LookupFromType<IFolderViewSettings>(),
        LookupFromType<IInitializeNetworkFolder>(),
        LookupFromType<INetworkFolderInternal>(),
        LookupFromType<IPreviewHandlerVisuals>(),
        LookupFromType<ICommDlgBrowser>(),
        LookupFromType<ICommDlgBrowser2>(),
        LookupFromType<IColumnManager>(),
        LookupFromType<IFolderFilterSite>(),
        LookupFromType<IFolderFilter>(),
        LookupFromType<IInputObjectSite>(),
        LookupFromType<IInputObject>(),
        LookupFromType<IInputObject2>(),
        LookupFromType<IShellIcon>(),
        LookupFromType<IShellBrowser>(),
        LookupFromType<IProfferService>(),
        LookupFromType<IShellItem>(),
        LookupFromType<IShellItem2>(),
        LookupFromType<IShellItemImageFactory>(),
        LookupFromType<IEnumShellItems>(),
        LookupFromType<ITransferAdviseSink>(),
        LookupFromType<ITransferSource>(),
        LookupFromType<IEnumResources>(),
        LookupFromType<IShellItemResources>(),
        LookupFromType<ITransferDestination>(),
        LookupFromType<IFileOperationProgressSink>(DebugInterestLevel::Interested),
        LookupFromType<IShellItemArray>(),
        LookupFromType<IInitializeWithItem>(),
        LookupFromType<IObjectWithSelection>(),
        LookupFromType<IObjectWithBackReferences>(),
        LookupFromType<IPropertyUI>(),
        LookupFromType<ICategoryProvider>(),
        LookupFromType<ICategorizer>(),
        LookupFromType<IDropTargetHelper>(DebugInterestLevel::Interested),
        LookupFromType<IDragSourceHelper>(DebugInterestLevel::Interested),
        LookupFromType<IShellLinkA>(),
        LookupFromType<IShellLinkW>(),
        LookupFromType<IShellLinkDataList>(),
        LookupFromType<IResolveShellLink>(),
        LookupFromType<IActionProgressDialog>(),
        LookupFromType<IActionProgress>(),
        LookupFromType<IShellExtInit>(DebugInterestLevel::Interested),
        LookupFromType<IShellPropSheetExt>(DebugInterestLevel::Interested),
        LookupFromType<IRemoteComputer>(),
        LookupFromType<IQueryContinue>(),
        LookupFromType<IObjectWithCancelEvent>(),
        LookupFromType<IUserNotification>(),
        LookupFromType<IItemNameLimits>(),
        LookupFromType<ISearchFolderItemFactory>(),
        LookupFromType<IExtractImage>(DebugInterestLevel::Interested),
        LookupFromType<IExtractImage2>(DebugInterestLevel::Interested),
        LookupFromType<IThumbnailHandlerFactory>(),
        LookupFromType<IParentAndItem>(),
        LookupFromType<IDockingWindow>(),
        LookupFromType<IDeskBand>(),
        LookupFromType<IDeskBandInfo>(),
        LookupFromType<ITaskbarList>(),
        LookupFromType<ITaskbarList2>(),
        LookupFromType<ITaskbarList3>(),
        LookupFromType<ITaskbarList4>(),
        LookupFromType<IExplorerBrowserEvents>(),
        LookupFromType<IExplorerBrowser>(),
        LookupFromType<IEnumObjects>(),
        LookupFromType<IOperationsProgressDialog>(),
        LookupFromType<IIOCancelInformation>(),
        LookupFromType<IFileOperation>(DebugInterestLevel::Interested),
        LookupFromType<IFileOperation2>(DebugInterestLevel::Interested),
        LookupFromType<IObjectProvider>(),
        LookupFromType<INamespaceWalkCB>(),
        LookupFromType<INamespaceWalkCB2>(),
        LookupFromType<INamespaceWalk>(),
        LookupFromType<IBandSite>(),
        LookupFromType<IModalWindow>(),
        LookupFromType<IContextMenuSite>(),
        LookupFromType<IMenuBand>(),
        LookupFromType<IRegTreeItem>(),
        LookupFromType<IDeskBar>(),
        LookupFromType<IMenuPopup>(),
        LookupFromType<IFileIsInUse>(),
        LookupFromType<IFileDialogEvents>(),
        LookupFromType<IFileDialog>(),
        LookupFromType<IFileSaveDialog>(),
        LookupFromType<IFileOpenDialog>(),
        LookupFromType<IFileDialogCustomize>(),
        LookupFromType<IApplicationAssociationRegistration>(),
        LookupFromType<IDelegateFolder>(),
        LookupFromType<IBrowserFrameOptions>(),
        LookupFromType<INewWindowManager>(),
        LookupFromType<IAttachmentExecute>(),
        LookupFromType<IShellMenuCallback>(),
        LookupFromType<IShellMenu>(),
        LookupFromType<IKnownFolder>(),
        LookupFromType<IKnownFolderManager>(),
        LookupFromType<ISharingConfigurationManager>(),
        LookupFromType<IRelatedItem>(),
        LookupFromType<IIdentityName>(),
        LookupFromType<IDelegateItem>(),
        LookupFromType<ICurrentItem>(),
        LookupFromType<ITransferMediumItem>(),
        LookupFromType<IDisplayItem>(),
        LookupFromType<IViewStateIdentityItem>(),
        LookupFromType<IPreviewItem>(),
        LookupFromType<IDestinationStreamFactory>(),
        LookupFromType<ICreateProcessInputs>(),
        LookupFromType<ICreatingProcess>(),
        LookupFromType<ILaunchUIContext>(),
        LookupFromType<ILaunchUIContextProvider>(),
        LookupFromType<INewMenuClient>(),
        LookupFromType<IInitializeWithBindCtx>(),
        LookupFromType<IShellItemFilter>(),
        LookupFromType<INameSpaceTreeControl>(),
        LookupFromType<INameSpaceTreeControlFolderCapabilities>(),
        LookupFromType<IPreviewHandler>(DebugInterestLevel::Interested),
        LookupFromType<IPreviewHandlerFrame>(),
        LookupFromType<IExplorerPaneVisibility>(),
        LookupFromType<IContextMenuCB>(DebugInterestLevel::Interested),
        LookupFromType<IDefaultExtractIconInit>(),
        LookupFromType<IExplorerCommand>(),
        LookupFromType<IExplorerCommandState>(),
        LookupFromType<IInitializeCommand>(),
        LookupFromType<IEnumExplorerCommand>(),
        LookupFromType<IExplorerCommandProvider>(),
        LookupFromType<IOpenControlPanel>(),
        LookupFromType<IFileSystemBindData>(),
        LookupFromType<IFileSystemBindData2>(),
        LookupFromType<ICustomDestinationList>(),
        LookupFromType<IApplicationDestinations>(),
        LookupFromType<IApplicationDocumentLists>(),
        LookupFromType<IObjectWithAppUserModelID>(),
        LookupFromType<IObjectWithProgID>(),
        LookupFromType<IUpdateIDList>(),
        LookupFromType<IDesktopWallpaper>(),
        LookupFromType<IHomeGroup>(),
        LookupFromType<IInitializeWithPropertyStore>(),
        LookupFromType<IOpenSearchSource>(),
        LookupFromType<IShellLibrary>(),
        LookupFromType<IDefaultFolderMenuInitialize>(),
        LookupFromType<IApplicationActivationManager>(),
        LookupFromType<IVirtualDesktopManager>(),
        LookupFromType<IAssocHandlerInvoker>(DebugInterestLevel::Interested),
        LookupFromType<IAssocHandler>(DebugInterestLevel::Interested),
        LookupFromType<IEnumAssocHandlers>(DebugInterestLevel::Interested),
        LookupFromType<IDataObjectProvider>(),
        LookupFromType<IDataTransferManagerInterop>(),
        LookupFromType<IFrameworkInputPaneHandler>(),
        LookupFromType<IFrameworkInputPane>(),
        LookupFromType<IAppVisibilityEvents>(),
        LookupFromType<IAppVisibility>(),
        LookupFromType<IPackageExecutionStateChangeNotification>(),
        LookupFromType<IPackageDebugSettings>(),
        LookupFromType<IPackageDebugSettings2>(),
        LookupFromType<ISuspensionDependencyManager>(),
        LookupFromType<IExecuteCommandApplicationHostEnvironment>(),
        LookupFromType<IExecuteCommandHost>(),
        LookupFromType<IApplicationDesignModeSettings>(),
        LookupFromType<IApplicationDesignModeSettings2>(),
        LookupFromType<ILaunchTargetMonitor>(),
        LookupFromType<ILaunchSourceViewSizePreference>(),
        LookupFromType<ILaunchTargetViewSizePreference>(),
        LookupFromType<ILaunchSourceAppUserModelId>(),
        LookupFromType<IInitializeWithWindow>(),
        LookupFromType<IHandlerInfo>(),
        LookupFromType<IHandlerInfo2>(),
        LookupFromType<IHandlerActivationHost>(),
        LookupFromType<IAppActivationUIInfo>(),
        LookupFromType<IContactManagerInterop>(),
        LookupFromType<IShellIconOverlayIdentifier>(),
        LookupFromType<IBannerNotificationHandler>(),
        LookupFromType<ISortColumnArray>(),
        LookupFromType<IPropertyKeyStore>(DebugInterestLevel::Interested),



        // ShlObj_core.h DECLARE_INTERFACE_IID_

        LookupFromType<IExtractIconA>(DebugInterestLevel::Normal),
        LookupFromType<IExtractIconW>(DebugInterestLevel::Normal),
        LookupFromType<IShellIconOverlayManager>(),
        LookupFromType<IShellIconOverlay>(),
        LookupFromType<IShellExecuteHookA>(),
        LookupFromType<IShellExecuteHookW>(),
        LookupFromType<IURLSearchHook>(),
        LookupFromType<ISearchContext>(),
        LookupFromType<IURLSearchHook2>(),
        LookupFromType<IShellDetails>(),
        LookupFromType<IObjMgr>(),
        LookupFromType<IACList>(),
        LookupFromType<IACList2>(),
        LookupFromType<IProgressDialog>(DebugInterestLevel::Interested),
        LookupFromType<IDockingWindowSite>(),
        LookupFromType<IShellChangeNotify>(),
        LookupFromType<IQueryInfo>(DebugInterestLevel::Interested),
        LookupFromType<IShellFolderViewCB>(),
        LookupFromType<IShellFolderView>(),
        LookupFromType<INamedPropertyBag>(),

        // ShlObj_core.h IRelatedItem

        LookupFromType<IIdentityName>(),
        LookupFromType<IDelegateItem>(),
        LookupFromType<ICurrentItem>(),
        LookupFromType<ITransferMediumItem>(),
        LookupFromType<IDisplayItem>(),
        LookupFromType<IViewStateIdentityItem>(),
        LookupFromType<IPreviewItem>(),

        // urlmon.h

        LookupFromType<IPersistMoniker>(),
        LookupFromType<IMonikerProp>(),
        LookupFromType<IBindProtocol>(),
        LookupFromType<IBinding>(),
        LookupFromType<IBindStatusCallback>(),
        LookupFromType<IAuthenticate>(),
        LookupFromType<IHttpNegotiate>(),
        LookupFromType<IWinInetFileStream>(),
        LookupFromType<IWindowForBindingUI>(),
        LookupFromType<IUri>(),
        LookupFromType<IUriContainer>(),
        LookupFromType<IUriBuilder>(),
        LookupFromType<IUriBuilderFactory>(),
        LookupFromType<IWinInetInfo>(),
        LookupFromType<IWinInetHttpTimeouts>(),
        LookupFromType<IWinInetCacheHints>(),
        LookupFromType<IBindHost>(),
        LookupFromType<IInternet>(),
        LookupFromType<IInternetBindInfo>(),
        LookupFromType<IInternetProtocolRoot>(),
        LookupFromType<IInternetProtocolSink>(),
        LookupFromType<IInternetProtocolSinkStackable>(),
        LookupFromType<IInternetSession>(),
        LookupFromType<IInternetThreadSwitch>(),
        LookupFromType<IInternetPriority>(),
        LookupFromType<IInternetProtocolInfo>(),
        LookupFromType<IInternetSecurityMgrSite>(),
        LookupFromType<IInternetSecurityManager>(),
        LookupFromType<IZoneIdentifier>(),
        LookupFromType<IInternetHostSecurityManager>(),
        LookupFromType<IInternetZoneManager>(),
        LookupFromType<ISoftDistExt>(),
        LookupFromType<ICatalogFileInfo>(),
        LookupFromType<IDataFilter>(),
        LookupFromType<IEncodingFilterFactory>(),
        LookupFromType<IWrappedProtocol>(),
        LookupFromType<IGetBindHandle>(),
        LookupFromType<IBindCallbackRedirect>(),
        LookupFromType<IBindHttpSecurity>(),


        // OCIdl.h IUnknown

        LookupFromType<IEnumConnections>(),
        LookupFromType<IConnectionPoint>(),
        LookupFromType<IEnumConnectionPoints>(),
        LookupFromType<IConnectionPointContainer>(),
        LookupFromType<IProvideClassInfo>(DebugInterestLevel::NotInterested),
        LookupFromType<IOleControl>(),
        LookupFromType<IOleControlSite>(),
        LookupFromType<IPropertyPage>(),
        LookupFromType<IPropertyPageSite>(),
        LookupFromType<IPropertyNotifySink>(),
        LookupFromType<ISpecifyPropertyPages>(),
        LookupFromType<ISimpleFrameSite>(),
        LookupFromType<IFont>(),
        LookupFromType<IPicture>(),
        LookupFromType<IOleUndoUnit>(),
        LookupFromType<IEnumOleUndoUnits>(),
        LookupFromType<IOleUndoManager>(),
        LookupFromType<IPointerInactive>(),
        LookupFromType<IObjectWithSite>(),
        LookupFromType<IPerPropertyBrowsing>(),
        LookupFromType<IQuickActivate>(),

        // ShlDisp.h

        LookupFromType<IFolderViewOC>(),
        LookupFromType<DShellFolderViewEvents>(),
        LookupFromType<DFConstraint>(),
        LookupFromType<FolderItem>(),
        LookupFromType<FolderItems>(),
        LookupFromType<FolderItemVerb>(),
        LookupFromType<FolderItemVerbs>(),
        LookupFromType<Folder>(DebugInterestLevel::Interested),
        LookupFromType<Folder2>(DebugInterestLevel::Interested),
        LookupFromType<Folder3>(DebugInterestLevel::Interested),
        LookupFromType<FolderItem2>(),
        LookupFromType<FolderItems2>(),
        LookupFromType<FolderItems3>(),
        LookupFromType<IShellLinkDual>(),
        LookupFromType<IShellLinkDual2>(),
        LookupFromType<IShellFolderViewDual>(),
        LookupFromType<IShellFolderViewDual2>(),
        LookupFromType<IShellFolderViewDual3>(),
        LookupFromType<IShellDispatch>(),
        LookupFromType<IShellDispatch2>(),
        LookupFromType<IShellDispatch3>(),
        LookupFromType<IShellDispatch4>(),
        LookupFromType<IShellDispatch5>(),
        LookupFromType<IShellDispatch6>(),
        LookupFromType<IFileSearchBand>(),
        LookupFromType<IWebWizardHost>(),
        LookupFromType<IWebWizardHost2>(),
        LookupFromType<INewWDEvents>(),
        LookupFromType<IAutoComplete>(),
        LookupFromType<IAutoComplete2>(),
        LookupFromType<IEnumACString>(),
        LookupFromType<IDataObjectAsyncCapability>(),



        // Unknwnbase.h
        LookupFromType<AsyncIUnknown>(),
        LookupFromType<IClassFactory>(),
        LookupFromType<IUnknown>(),





        LookupFromText("IID_Unknown_2", L"{CAD9AE9F-56E2-40F1-AFB6-3813E320DCFD}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_3", L"{42339A50-7A91-44F9-87AC-37E6EC1B1A88}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_4", L"{FDBEE76E-F12B-408E-93AB-9BE8521000D9}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_5", L"{127F6ACB-7E78-4368-83A4-ED1DE72BACA6}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_6", L"{00000039-0000-0000-C000-000000000046}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_7", L"{334D391F-0E79-3B15-C9FF-EAC65DD07C42}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_8", L"{6F3094CB-FFC0-4CF8-B52E-7E9A810B6CD5}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_9", L"{77DD1250-139C-2BC3-BD95-900ACED61BE5}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_10", L"{BFD60505-5A1F-4E41-88BA-A6FB07202DA9}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_11", L"{4F4F92B5-6DED-4E9B-A93F-013891B3A8B7}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_12", L"{9BC79C93-2289-4BB5-ABF4-3287FD9CAE39}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_13", L"{11456F96-09D1-4909-8F36-4EB74E42B93E}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_14", L"{2132B005-C604-4354-85BD-8F2E24181B0C}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_15", L"{03FB5C57-D534-45F5-A1F4-D39556983875}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_16", L"{2C258AE7-50DC-49FF-9D1D-2ECB9A52CDD7}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_17", L"{4C1E39E1-E3E3-4296-AA86-EC938D896E92}", DebugInterestLevel::NotInterested), // This is an MMC private interface used to support property sheets in a managed(MCF) snap - in. https://microsoft.public.management.mmc.narkive.com/YqDkkOuN/queried-for-4c1e39e1-e3e3-4296-aa86-ec938d896e92-interface
        LookupFromText("IID_Unknown_18", L"{6C72B11B-DBE0-4C87-B1A8-7C8A36BD563D}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_Unknown_19", L"{E0EECF93-8B48-44AF-A377-07852487B85C}", DebugInterestLevel::NotInterested),

        // https://gist.github.com/olafhartong/980e9cd51925ff06a5a3fdfb24fb96c2

        LookupFromText("IID_IContextMenuForProgInvoke", L"{649E2263-DC09-466F-9D66-3EB133EE8F81}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_IAudioDeviceGraph", L"{3C169FF7-37B2-484C-B199-C3155590F316}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_ILibraryDescription", L"{86187C37-E662-4D1E-A122-7478676D7E6E}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_IManagedObject", L"{C3FCC19E-A970-11D2-8B5A-00A0C9B7C9C4}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_IPropertyInfoProvider", L"{089f3011-bb5c-4f9c-9b8f-9a67ed446e91}"),
        LookupFromText("IID_IPropertyStoreCache", L"{3017056D-9A91-4E90-937D-746C72ABBF4F}"),
        LookupFromText("IID_IPropertyStoreFactory", L"{BC110B6D-57E8-4148-A9C6-91015AB2F3A5}"),
        LookupFromText("IID_IPropertyStore", L"{886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99}"), // https://www.pinvoke.net/default.aspx/Interfaces/IPropertyStore.html

        LookupFromType<IInspectable>(DebugInterestLevel::NotInterested),
        LookupFromType<IQueryAssociations>(),

        LookupFromText("IID_IDataObject", L"{3CEE8CC1-1ADB-327F-9B97-7A9C8089BFB3}"), // https://microsoft.public.platformsdk.shell.narkive.com/t6GVO0vR/sendmail-in-xp
        LookupFromText("IID_IViewResultRelatedItem", L"{50BC72DA-9633-47CB-80AC-727661FB9B9F}", DebugInterestLevel::NotInterested),
        LookupFromText("IID_IFolderViewCapabilities", L"{7B88EA95-1C91-42AA-BAE5-6D730CBEC794}", DebugInterestLevel::NotInterested),
        LookupFromText("CIconAndThumbnailOplockWrapper", L"{2968087C-7490-430F-BB8B-2156610D825A}", DebugInterestLevel::NotInterested),


        // general ref https://gist.github.com/invokethreatguy/b2482f4204d2e71dcb5f9a081ccf7baf
    // https://www.magnumdb.com/search?q=value%3A%227b88ea95-1c91-42aa-bae5-6d730cbec794%22

#pragma pop_macro("DEFINE_GUID")
};

std::string GetDebugLogName() {
    char fname[512];
    GetModuleFileNameA(nullptr, fname, 511);

    return std::filesystem::path(fname).filename().string();
}

std::ofstream logFile = std::ofstream(std::format("P:\\PboEx_{}.log", GetDebugLogName()), std::ofstream::app | std::ofstream::out);
std::ofstream logFileBad = std::ofstream(std::format("P:\\PboEx_{}_Bad.log", GetDebugLogName()), std::ofstream::app | std::ofstream::out);

void DebugLogger::OnQueryInterfaceEntry(const GUID& riid, const std::source_location location, const char* funcName)
{
    auto guidName = GetGUIDName(riid);
    if (guidName.second == DebugInterestLevel::NotInterested)
        return;

    auto prnt = std::format("[{:%T}] Trace - {} - {}\n", std::chrono::system_clock::now(), funcName, guidName.first);
    OutputDebugStringA(prnt.c_str());
    //logFile.write(prnt.c_str(), prnt.length());
}



void DebugLogger::OnQueryInterfaceExitUnhandled(const GUID& riid, const std::source_location location, const char* funcName)
{
    auto guidName = GetGUIDName(riid);
    auto prnt = std::format("[{:%T}] Unimplemented GUID - {} - {}\n", std::chrono::system_clock::now(), funcName, guidName.first);
    if (guidName.second == DebugInterestLevel::NotInterested)
        return;

    OutputDebugStringA(prnt.c_str());
    logFile.write(prnt.c_str(), prnt.length());
    logFile.flush();

    if (guidName.second == DebugInterestLevel::Interested) {
        logFileBad.write(prnt.c_str(), prnt.length());
        logFileBad.flush();


#ifdef ENABLE_SENTRY
        prnt = std::format("Unimplemented GUID - {} - {}\n", funcName, guidName.first);

        auto event = sentry_value_new_message_event(
            /*   level */ SENTRY_LEVEL_WARNING,
            /*  logger */ nullptr,
            /* message */ prnt.c_str()
        );

        auto thread = sentry_value_new_thread(GetCurrentThreadId(), "name");
        sentry_value_set_by_key(thread, "stacktrace", sentry_value_new_stacktrace(nullptr, 16));
        sentry_event_add_thread(event, thread);

        sentry_capture_event(event);
#endif

    }
}

//#TODO take string view
void DebugLogger::TraceLog(const std::string& message, const std::source_location location, const char* funcName)
{
    auto prnt = std::format("[{:%T}] T [{}] - {}\n", std::chrono::system_clock::now(), funcName, message);
    OutputDebugStringA(prnt.c_str());
    logFile.write(prnt.c_str(), prnt.length());
    logFile.flush();
}

void DebugLogger::TraceLog(const std::wstring& message, const std::source_location location, const char* funcName)
{
    auto prnt = std::format("[{:%T}] T [{}] - {}\n", std::chrono::system_clock::now(), funcName, UTF8::Encode(message));
    OutputDebugStringA(prnt.c_str());
    logFile.write(prnt.c_str(), prnt.length());
    logFile.flush();
}

void DebugLogger::WarnLog(const std::string& message, const std::source_location location, const char* funcName)
{
    auto prnt = std::format("[{:%T}] W [{}] - {}\n", std::chrono::system_clock::now(), funcName, message);
    OutputDebugStringA(prnt.c_str());
    logFile.write(prnt.c_str(), prnt.length());
    logFile.flush();
    logFileBad.write(prnt.c_str(), prnt.length());
    logFileBad.flush();

#ifdef ENABLE_SENTRY
    prnt = std::format("W [{}] - {}\n", funcName, message);

    auto event = sentry_value_new_message_event(
        /*   level */ SENTRY_LEVEL_WARNING,
        /*  logger */ nullptr,
        /* message */ prnt.c_str()
    );

    auto thread = sentry_value_new_thread(GetCurrentThreadId(), "name");
    sentry_value_set_by_key(thread, "stacktrace", sentry_value_new_stacktrace(nullptr, 16));
    sentry_event_add_thread(event, thread);

    sentry_capture_event(event);
#endif
}

void DebugLogger::CaptureException(std::exception& exception, std::map<std::string, std::string> attributes)
{
#ifdef ENABLE_SENTRY
    sentry_value_t event = sentry_value_new_event();

    sentry_value_t exc = sentry_value_new_exception(typeid(exception).name(), exception.what());
    sentry_event_add_exception(event, exc);

    //#TODO but this is windows 10 only
    /*
     HRESULT GetThreadDescription(
      [in]  HANDLE hThread,
      [out] PWSTR  *ppszThreadDescription
    );
     */

    auto thread = sentry_value_new_thread(GetCurrentThreadId(), "name");
    sentry_value_set_by_key(thread, "stacktrace", sentry_value_new_stacktrace(nullptr, 16));
    sentry_event_add_thread(event, thread);

    if (!attributes.empty()) {
        sentry_value_t attribs = sentry_value_new_object();
        for (auto& [key, value] : attributes) {
            sentry_value_set_by_key(attribs, key.c_str(), sentry_value_new_string(value.c_str()));
        }

        sentry_value_set_by_key(event, "extra", attribs); // "extra" must be named like that https://develop.sentry.dev/sdk/event-payloads/
    }

    sentry_capture_event(event);
#endif
}

bool DebugLogger::IsIIDUninteresting(const GUID& riid)
{
    return GetGUIDName(riid).second == DebugInterestLevel::NotInterested;
}

LookupInfoT DebugLogger::GetGUIDName(const GUID& guid)
{
    auto found = guidLookupTable.find(guid);

    if (found != guidLookupTable.end()) {
        return found->second;
    }
    else {
        wchar_t* guidString;
        StringFromCLSID(guid, &guidString);
        auto guidName = UTF8::Encode(guidString);
        ::CoTaskMemFree(guidString);

        // Every entry must be in lookup table, because we return stringview
        auto inserted = guidLookupTable.insert({ guid, { guidName, DebugInterestLevel::Interested } }); // we are very interested in unknown GUIDs

        return inserted->second;
    }
}

