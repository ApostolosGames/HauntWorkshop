// HAUNT Workshop Uploader: a small window that publishes a packaged map to the Steam Workshop.
//
// SteamCMD's workshop_build_item cannot set Workshop tags, and HAUNT only lists items
// tagged "Map", so this goes through the Steamworks API instead and always sets the tag.
// Needs the Steam client running and signed in to an account that owns HAUNT.

#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
namespace Gdiplus
{
	using std::max;
	using std::min;
}
#include <gdiplus.h>

#include <steam/steam_api.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' " \
	"version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;

namespace
{
	constexpr AppId_t HauntAppId = 3175590;
	constexpr const char* MapTag = "Map";

	// Steam rejects larger preview images outright.
	constexpr uintmax_t MaxPreviewBytes = 1024 * 1024;
	constexpr size_t MaxTitleChars = 128;
	constexpr size_t MaxDescriptionChars = 8000;

	constexpr UINT_PTR SteamTimer = 1;

	enum ControlId
	{
		IdFolderEdit = 100,
		IdFolderBrowse,
		IdFolderStatus,
		IdImageEdit,
		IdImageBrowse,
		IdImageStatus,
		IdTitleEdit,
		IdDescriptionEdit,
		IdVisibilityCombo,
		IdChangeNoteEdit,
		IdItemIdEdit,
		IdProgress,
		IdStatus,
		IdUpload,
		IdOpenPage,
	};

	enum class UploadState
	{
		Idle,
		Creating,
		Submitting,
	};

	struct VisibilityOption
	{
		const wchar_t* Label;
		ERemoteStoragePublishedFileVisibility Value;
	};

	const VisibilityOption VisibilityOptions[] = {
		{ L"Private (only you)", k_ERemoteStoragePublishedFileVisibilityPrivate },
		{ L"Friends only", k_ERemoteStoragePublishedFileVisibilityFriendsOnly },
		{ L"Unlisted (anyone with the link)", k_ERemoteStoragePublishedFileVisibilityUnlisted },
		{ L"Public", k_ERemoteStoragePublishedFileVisibilityPublic },
	};

	struct App
	{
		HWND Window = nullptr;
		HFONT Font = nullptr;
		UINT Dpi = 96;

		RECT PreviewRect{};
		std::unique_ptr<Gdiplus::Bitmap> Preview;

		// Text colour per status label; red for problems, green once a check passes.
		COLORREF FolderColor = 0;
		COLORREF ImageColor = 0;
		COLORREF StatusColor = 0;

		bool bSteamReady = false;
		UploadState State = UploadState::Idle;
		SteamAPICall_t PendingCall = k_uAPICallInvalid;
		UGCUpdateHandle_t UpdateHandle = k_UGCUpdateHandleInvalid;
		PublishedFileId_t ItemId = 0;
		bool bNewItem = false;
		std::wstring PageUrl;
	};

	App G;

	constexpr COLORREF ErrorColor = RGB(196, 43, 28);
	constexpr COLORREF OkColor = RGB(16, 124, 16);

	int Scale(int Value)
	{
		return MulDiv(Value, static_cast<int>(G.Dpi), 96);
	}

	HWND Control(int Id)
	{
		return GetDlgItem(G.Window, Id);
	}

	std::wstring GetText(int Id)
	{
		const HWND Hwnd = Control(Id);
		std::wstring Text(static_cast<size_t>(GetWindowTextLengthW(Hwnd)) + 1, L'\0');
		GetWindowTextW(Hwnd, Text.data(), static_cast<int>(Text.size()));
		Text.resize(Text.size() - 1);
		return Text;
	}

	std::wstring Trim(const std::wstring& Text)
	{
		const size_t First = Text.find_first_not_of(L" \t\r\n\"");
		if (First == std::wstring::npos)
		{
			return L"";
		}
		return Text.substr(First, Text.find_last_not_of(L" \t\r\n\"") - First + 1);
	}

	std::string ToUtf8(const std::wstring& Text)
	{
		if (Text.empty())
		{
			return {};
		}
		const int Size = WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), static_cast<int>(Text.size()), nullptr, 0, nullptr, nullptr);
		std::string Out(Size, '\0');
		WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), static_cast<int>(Text.size()), Out.data(), Size, nullptr, nullptr);
		return Out;
	}

	std::wstring FromUtf8(const char* Text)
	{
		const int Size = MultiByteToWideChar(CP_UTF8, 0, Text, -1, nullptr, 0);
		std::wstring Out(Size > 0 ? Size - 1 : 0, L'\0');
		if (Size > 1)
		{
			MultiByteToWideChar(CP_UTF8, 0, Text, -1, Out.data(), Size);
		}
		return Out;
	}

	void SetLabel(int Id, COLORREF& ColorSlot, COLORREF Color, const std::wstring& Text)
	{
		ColorSlot = Color;
		SetWindowTextW(Control(Id), Text.c_str());
		InvalidateRect(Control(Id), nullptr, TRUE);
	}

	void SetStatus(const std::wstring& Text, bool bError = false)
	{
		SetLabel(IdStatus, G.StatusColor, bError ? ErrorColor : GetSysColor(COLOR_WINDOWTEXT), Text);
	}

	// ---------------------------------------------------------------------------------
	// Settings: the last values used, so an update is just "change note, Upload".

	std::wstring SettingsPath()
	{
		PWSTR Base = nullptr;
		std::wstring Path;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &Base)))
		{
			Path = std::wstring(Base) + L"\\HauntWorkshopUploader";
			CreateDirectoryW(Path.c_str(), nullptr);
			Path += L"\\settings.ini";
		}
		CoTaskMemFree(Base);
		return Path;
	}

	// INI values are single-line, so newlines in the description are escaped.
	std::wstring Escape(const std::wstring& Text)
	{
		std::wstring Out;
		for (size_t i = 0; i < Text.size(); ++i)
		{
			if (Text[i] == L'\\')
			{
				Out += L"\\\\";
			}
			else if (Text[i] == L'\r' && i + 1 < Text.size() && Text[i + 1] == L'\n')
			{
				Out += L"\\n";
				++i;
			}
			else if (Text[i] == L'\n')
			{
				Out += L"\\n";
			}
			else
			{
				Out += Text[i];
			}
		}
		return Out;
	}

	std::wstring Unescape(const std::wstring& Text)
	{
		std::wstring Out;
		for (size_t i = 0; i < Text.size(); ++i)
		{
			if (Text[i] == L'\\' && i + 1 < Text.size())
			{
				Out += Text[i + 1] == L'n' ? L"\r\n" : std::wstring(1, Text[i + 1]);
				++i;
			}
			else
			{
				Out += Text[i];
			}
		}
		return Out;
	}

	std::wstring ReadSetting(const wchar_t* Key)
	{
		std::vector<wchar_t> Buffer(32768);
		GetPrivateProfileStringW(L"Upload", Key, L"", Buffer.data(), static_cast<DWORD>(Buffer.size()), SettingsPath().c_str());
		return Unescape(Buffer.data());
	}

	void WriteSetting(const wchar_t* Key, const std::wstring& Value)
	{
		// Quoted so leading/trailing spaces survive; GetPrivateProfileString strips the quotes.
		WritePrivateProfileStringW(L"Upload", Key, (L"\"" + Escape(Value) + L"\"").c_str(), SettingsPath().c_str());
	}

	void SaveSettings()
	{
		WriteSetting(L"ContentFolder", GetText(IdFolderEdit));
		WriteSetting(L"PreviewImage", GetText(IdImageEdit));
		WriteSetting(L"Title", GetText(IdTitleEdit));
		WriteSetting(L"Description", GetText(IdDescriptionEdit));
		WriteSetting(L"ItemId", GetText(IdItemIdEdit));
		WriteSetting(L"Visibility",
			std::to_wstring(SendMessageW(Control(IdVisibilityCombo), CB_GETCURSEL, 0, 0)));
	}

	// ---------------------------------------------------------------------------------
	// Checks

	// HAUNT refuses anything but exactly one .pak with its matching .utoc/.ucas, so
	// catching it here saves an upload that would never load.
	std::wstring CheckContentFolder(const std::wstring& Folder)
	{
		if (Folder.empty())
		{
			return L"Choose the folder holding your packaged .pak, .utoc and .ucas.";
		}
		std::error_code Error;
		if (!fs::is_directory(Folder, Error))
		{
			return L"That folder does not exist.";
		}

		std::vector<fs::path> Paks;
		std::vector<std::wstring> Names;
		for (const fs::directory_entry& Entry : fs::directory_iterator(Folder, Error))
		{
			Names.push_back(Entry.path().filename().wstring());
			if (Entry.is_regular_file() && _wcsicmp(Entry.path().extension().c_str(), L".pak") == 0)
			{
				Paks.push_back(Entry.path());
			}
		}
		if (Paks.size() != 1)
		{
			return L"The folder must contain exactly one .pak (found " + std::to_wstring(Paks.size()) + L").";
		}

		const std::wstring Stem = Paks[0].stem().wstring();
		if (Stem.size() >= 2 && _wcsicmp(Stem.c_str() + Stem.size() - 2, L"_P") == 0)
		{
			return L"'" + Stem + L".pak' ends in _P, which HAUNT reserves for patches. Rename it.";
		}
		for (const wchar_t* Ext : { L".utoc", L".ucas" })
		{
			if (!fs::is_regular_file(fs::path(Folder) / (Stem + Ext), Error))
			{
				return L"Missing " + Stem + Ext + L" next to the .pak.";
			}
		}
		for (const std::wstring& Name : Names)
		{
			if (_wcsicmp(Name.c_str(), (Stem + L".pak").c_str()) != 0
				&& _wcsicmp(Name.c_str(), (Stem + L".utoc").c_str()) != 0
				&& _wcsicmp(Name.c_str(), (Stem + L".ucas").c_str()) != 0)
			{
				return L"Remove '" + Name + L"': the folder may only hold the three pak files.";
			}
		}
		return L"";
	}

	// Loads into memory so GDI+ does not keep the file locked while Steam reads it.
	std::unique_ptr<Gdiplus::Bitmap> LoadImageCopy(const std::wstring& Path)
	{
		Gdiplus::Bitmap Source(Path.c_str());
		if (Source.GetLastStatus() != Gdiplus::Ok || Source.GetWidth() == 0)
		{
			return nullptr;
		}
		auto Copy = std::make_unique<Gdiplus::Bitmap>(Source.GetWidth(), Source.GetHeight(), PixelFormat32bppARGB);
		Gdiplus::Graphics(Copy.get()).DrawImage(&Source, 0, 0, Source.GetWidth(), Source.GetHeight());
		return Copy;
	}

	std::wstring CheckImage(const std::wstring& Path)
	{
		if (Path.empty())
		{
			return L"Choose a JPG, PNG or GIF, under 1 MB. 16:9 looks best.";
		}
		std::error_code Error;
		if (!fs::is_regular_file(Path, Error))
		{
			return L"That file does not exist.";
		}
		if (!G.Preview)
		{
			return L"That file is not an image Windows can read.";
		}
		if (fs::file_size(Path, Error) > MaxPreviewBytes)
		{
			return L"The image is over 1 MB; Steam will reject it. Save it smaller.";
		}
		return L"";
	}

	void RefreshFolder()
	{
		const std::wstring Folder = Trim(GetText(IdFolderEdit));
		const std::wstring Problem = CheckContentFolder(Folder);
		if (Problem.empty())
		{
			SetLabel(IdFolderStatus, G.FolderColor, OkColor, L"Looks good: one .pak with its .utoc and .ucas.");
		}
		else
		{
			SetLabel(IdFolderStatus, G.FolderColor, Folder.empty() ? GetSysColor(COLOR_GRAYTEXT) : ErrorColor, Problem);
		}
	}

	void RefreshImage()
	{
		const std::wstring Path = Trim(GetText(IdImageEdit));
		std::error_code Error;
		G.Preview = fs::is_regular_file(Path, Error) ? LoadImageCopy(Path) : nullptr;
		InvalidateRect(G.Window, &G.PreviewRect, TRUE);

		const std::wstring Problem = CheckImage(Path);
		if (Problem.empty())
		{
			SetLabel(IdImageStatus, G.ImageColor, OkColor,
				std::to_wstring(G.Preview->GetWidth()) + L" x " + std::to_wstring(G.Preview->GetHeight())
				+ L", " + std::to_wstring((fs::file_size(Path, Error) + 1023) / 1024) + L" KB");
		}
		else
		{
			SetLabel(IdImageStatus, G.ImageColor, Path.empty() ? GetSysColor(COLOR_GRAYTEXT) : ErrorColor, Problem);
		}
	}

	// ---------------------------------------------------------------------------------
	// Steam

	std::wstring DescribeResult(EResult Result)
	{
		switch (Result)
		{
		case k_EResultNotLoggedOn: return L"You are not signed in to Steam.";
		case k_EResultNoConnection: return L"Steam could not be reached. Check your connection.";
		case k_EResultAccessDenied:
		case k_EResultInsufficientPrivilege:
			return L"Steam refused the upload. If you entered an item ID, it must be an item you own; "
				   L"otherwise this account may be limited from posting Workshop content.";
		case k_EResultBanned: return L"This account is banned from submitting Workshop content.";
		case k_EResultTimeout: return L"Steam timed out. Try again in a moment.";
		case k_EResultLimitExceeded: return L"Your Steam Cloud storage is full.";
		case k_EResultRateLimitExceeded: return L"Too many uploads too quickly. Wait a few minutes.";
		case k_EResultFileNotFound: return L"Steam could not find that item or the files. Check the item ID and folder.";
		case k_EResultServiceUnavailable: return L"The Steam Workshop is unavailable right now.";
		case k_EResultInvalidParam: return L"Steam rejected the item's details (title, image or files).";
		default: return L"Steam returned error " + std::to_wstring(static_cast<int>(Result)) + L".";
		}
	}

	bool EnsureSteam()
	{
		if (G.bSteamReady)
		{
			return true;
		}
		// Saves shipping a steam_appid.txt next to the exe.
		SetEnvironmentVariableW(L"SteamAppId", std::to_wstring(HauntAppId).c_str());
		SetEnvironmentVariableW(L"SteamGameId", std::to_wstring(HauntAppId).c_str());

		SteamErrMsg Error{};
		if (SteamAPI_InitEx(&Error) != k_ESteamAPIInitResult_OK)
		{
			SetStatus(L"Could not connect to Steam: " + FromUtf8(Error)
				+ L"\r\nStart Steam and sign in with the account that owns HAUNT.", true);
			return false;
		}
		G.bSteamReady = true;
		SetTimer(G.Window, SteamTimer, 100, nullptr);
		return true;
	}

	void SetBusy(bool bBusy)
	{
		for (int Id : { IdFolderEdit, IdFolderBrowse, IdImageEdit, IdImageBrowse, IdTitleEdit, IdDescriptionEdit,
				 IdVisibilityCombo, IdChangeNoteEdit, IdItemIdEdit, IdUpload })
		{
			EnableWindow(Control(Id), !bBusy);
		}
		SendMessageW(Control(IdProgress), PBM_SETPOS, 0, 0);
	}

	void Finish(const std::wstring& Message, bool bError)
	{
		G.State = UploadState::Idle;
		G.PendingCall = k_uAPICallInvalid;
		G.UpdateHandle = k_UGCUpdateHandleInvalid;
		SetBusy(false);
		SetStatus(Message, bError);
		if (!bError)
		{
			SendMessageW(Control(IdProgress), PBM_SETPOS, 1000, 0);
		}
	}

	void SubmitUpdate()
	{
		G.UpdateHandle = SteamUGC()->StartItemUpdate(HauntAppId, G.ItemId);
		if (G.UpdateHandle == k_UGCUpdateHandleInvalid)
		{
			Finish(L"Steam would not start the update.", true);
			return;
		}

		const std::wstring Title = Trim(GetText(IdTitleEdit));
		const std::wstring Description = GetText(IdDescriptionEdit);
		const std::wstring Image = Trim(GetText(IdImageEdit));

		// On an update, an empty title or description leaves the one on Steam alone.
		if (!Title.empty())
		{
			SteamUGC()->SetItemTitle(G.UpdateHandle, ToUtf8(Title).c_str());
		}
		if (!Trim(Description).empty())
		{
			SteamUGC()->SetItemDescription(G.UpdateHandle, ToUtf8(Description).c_str());
		}
		const LRESULT Visibility = SendMessageW(Control(IdVisibilityCombo), CB_GETCURSEL, 0, 0);
		SteamUGC()->SetItemVisibility(G.UpdateHandle, VisibilityOptions[Visibility < 0 ? 0 : Visibility].Value);
		SteamUGC()->SetItemContent(G.UpdateHandle, ToUtf8(fs::absolute(Trim(GetText(IdFolderEdit))).wstring()).c_str());
		if (!Image.empty())
		{
			SteamUGC()->SetItemPreview(G.UpdateHandle, ToUtf8(fs::absolute(Image).wstring()).c_str());
		}

		// Always set: HAUNT only lists items tagged Map, and players should not have to know that.
		const char* Tags[] = { MapTag };
		SteamParamStringArray_t TagArray{ Tags, 1 };
		SteamUGC()->SetItemTags(G.UpdateHandle, &TagArray);

		G.PendingCall = SteamUGC()->SubmitItemUpdate(G.UpdateHandle, ToUtf8(GetText(IdChangeNoteEdit)).c_str());
		G.State = UploadState::Submitting;
		SetStatus(L"Preparing upload...");
	}

	void StartUpload()
	{
		const std::wstring FolderProblem = CheckContentFolder(Trim(GetText(IdFolderEdit)));
		const std::wstring ImagePath = Trim(GetText(IdImageEdit));
		const std::wstring Title = Trim(GetText(IdTitleEdit));
		const std::wstring IdText = Trim(GetText(IdItemIdEdit));
		const PublishedFileId_t ItemId = IdText.empty() ? 0 : _wcstoui64(IdText.c_str(), nullptr, 10);

		if (!FolderProblem.empty())
		{
			SetStatus(L"Map folder: " + FolderProblem, true);
			return;
		}
		// A new item needs an image; an update may keep the one already on Steam.
		if (!ImagePath.empty() || ItemId == 0)
		{
			const std::wstring ImageProblem = CheckImage(ImagePath);
			if (!ImageProblem.empty())
			{
				SetStatus(L"Preview image: " + ImageProblem, true);
				return;
			}
		}
		if (ItemId == 0 && Title.empty())
		{
			SetStatus(L"Give the map a title.", true);
			return;
		}
		if (!EnsureSteam())
		{
			return;
		}

		SaveSettings();
		SetBusy(true);
		ShowWindow(Control(IdOpenPage), SW_HIDE);
		G.ItemId = ItemId;
		G.bNewItem = ItemId == 0;

		if (G.bNewItem)
		{
			G.PendingCall = SteamUGC()->CreateItem(HauntAppId, k_EWorkshopFileTypeCommunity);
			G.State = UploadState::Creating;
			SetStatus(L"Creating the Workshop item...");
		}
		else
		{
			SubmitUpdate();
		}
	}

	void ShowPageButton(const std::wstring& Label, const std::wstring& Url)
	{
		G.PageUrl = Url;
		SetWindowTextW(Control(IdOpenPage), Label.c_str());
		ShowWindow(Control(IdOpenPage), SW_SHOW);
	}

	void PollSteam()
	{
		SteamAPI_RunCallbacks();
		if (G.State == UploadState::Idle)
		{
			return;
		}

		bool bIoFailed = false;
		if (!SteamUtils()->IsAPICallCompleted(G.PendingCall, &bIoFailed))
		{
			if (G.State == UploadState::Submitting)
			{
				uint64 Done = 0, Total = 0;
				const EItemUpdateStatus Status = SteamUGC()->GetItemUpdateProgress(G.UpdateHandle, &Done, &Total);
				const wchar_t* Stage = L"Working...";
				switch (Status)
				{
				case k_EItemUpdateStatusPreparingConfig: Stage = L"Preparing..."; break;
				case k_EItemUpdateStatusPreparingContent: Stage = L"Compressing the map..."; break;
				case k_EItemUpdateStatusUploadingContent: Stage = L"Uploading the map..."; break;
				case k_EItemUpdateStatusUploadingPreviewFile: Stage = L"Uploading the preview image..."; break;
				case k_EItemUpdateStatusCommittingChanges: Stage = L"Publishing..."; break;
				default: break;
				}
				if (Status != k_EItemUpdateStatusInvalid)
				{
					const int Permille = Total ? static_cast<int>(Done * 1000 / Total) : 0;
					SendMessageW(Control(IdProgress), PBM_SETPOS, Permille, 0);
					std::wstring Text = Stage;
					if (Total)
					{
						Text += L"  " + std::to_wstring(Done / (1024 * 1024)) + L" / " + std::to_wstring(Total / (1024 * 1024)) + L" MB";
					}
					SetStatus(Text);
				}
			}
			return;
		}

		if (G.State == UploadState::Creating)
		{
			CreateItemResult_t Result{};
			bool bFailed = bIoFailed;
			if (!bFailed)
			{
				SteamUtils()->GetAPICallResult(G.PendingCall, &Result, sizeof(Result), CreateItemResult_t::k_iCallback, &bFailed);
			}
			if (bFailed || Result.m_eResult != k_EResultOK)
			{
				Finish(L"Could not create the Workshop item. " + (bFailed ? std::wstring(L"Steam did not respond.") : DescribeResult(Result.m_eResult)), true);
				return;
			}
			// Saved straight away: the item exists on Steam now even if the upload fails,
			// and retrying should update it rather than create another.
			G.ItemId = Result.m_nPublishedFileId;
			SetWindowTextW(Control(IdItemIdEdit), std::to_wstring(G.ItemId).c_str());
			SaveSettings();
			SubmitUpdate();
			return;
		}

		SubmitItemUpdateResult_t Result{};
		bool bFailed = bIoFailed;
		if (!bFailed)
		{
			SteamUtils()->GetAPICallResult(G.PendingCall, &Result, sizeof(Result), SubmitItemUpdateResult_t::k_iCallback, &bFailed);
		}
		if (bFailed || Result.m_eResult != k_EResultOK)
		{
			std::wstring Message = L"Upload failed. " + (bFailed ? std::wstring(L"Steam did not respond.") : DescribeResult(Result.m_eResult));
			if (G.bNewItem)
			{
				Message += L"\r\nThe item was created; pressing Upload again will update it.";
			}
			Finish(Message, true);
			return;
		}

		if (Result.m_bUserNeedsToAcceptWorkshopLegalAgreement)
		{
			Finish(L"Uploaded, but the item stays hidden until you accept the Steam Workshop legal agreement.", true);
			ShowPageButton(L"Open agreement", L"https://steamcommunity.com/sharedfiles/workshoplegalagreement");
			return;
		}

		Finish(L"Uploaded item " + std::to_wstring(G.ItemId) + L". Subscribe to it in Steam to test it in HAUNT.", false);
		ShowPageButton(L"Open item page", L"https://steamcommunity.com/sharedfiles/filedetails/?id=" + std::to_wstring(G.ItemId));
	}

	// ---------------------------------------------------------------------------------
	// Pickers

	std::wstring PickPath(bool bFolder, const std::wstring& Current)
	{
		std::wstring Result;
		IFileOpenDialog* Dialog = nullptr;
		if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Dialog))))
		{
			return Result;
		}

		DWORD Options = 0;
		Dialog->GetOptions(&Options);
		Dialog->SetOptions(Options | FOS_FORCEFILESYSTEM | (bFolder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
		if (bFolder)
		{
			Dialog->SetTitle(L"Choose the folder with your .pak, .utoc and .ucas");
		}
		else
		{
			const COMDLG_FILTERSPEC Filters[] = {
				{ L"Images (*.jpg, *.png, *.gif)", L"*.jpg;*.jpeg;*.png;*.gif" },
				{ L"All files", L"*.*" },
			};
			Dialog->SetFileTypes(2, Filters);
			Dialog->SetTitle(L"Choose the Workshop preview image");
		}

		// Open where the current value points, so re-picking does not start from scratch.
		std::error_code Error;
		const fs::path Start = bFolder ? fs::path(Current) : fs::path(Current).parent_path();
		IShellItem* StartItem = nullptr;
		if (!Current.empty() && fs::is_directory(Start, Error)
			&& SUCCEEDED(SHCreateItemFromParsingName(fs::absolute(Start).c_str(), nullptr, IID_PPV_ARGS(&StartItem))))
		{
			Dialog->SetFolder(StartItem);
			StartItem->Release();
		}

		IShellItem* Picked = nullptr;
		PWSTR Path = nullptr;
		if (SUCCEEDED(Dialog->Show(G.Window)) && SUCCEEDED(Dialog->GetResult(&Picked))
			&& SUCCEEDED(Picked->GetDisplayName(SIGDN_FILESYSPATH, &Path)))
		{
			Result = Path;
			CoTaskMemFree(Path);
		}
		if (Picked)
		{
			Picked->Release();
		}
		Dialog->Release();
		return Result;
	}

	// ---------------------------------------------------------------------------------
	// Window. Layout is in 96-DPI units and scaled once at creation.

	HWND Make(const wchar_t* Class, const wchar_t* Text, DWORD Style, int X, int Y, int W, int H, int Id)
	{
		return CreateWindowExW(0, Class, Text, WS_CHILD | WS_VISIBLE | Style,
			Scale(X), Scale(Y), Scale(W), Scale(H), G.Window,
			reinterpret_cast<HMENU>(static_cast<INT_PTR>(Id)), GetModuleHandleW(nullptr), nullptr);
	}

	void Label(const wchar_t* Text, int Y)
	{
		Make(L"STATIC", Text, SS_RIGHT, 16, Y + 4, 112, 20, -1);
	}

	void Edit(int Id, int X, int Y, int W, DWORD Extra = ES_AUTOHSCROLL)
	{
		Make(L"EDIT", L"", WS_TABSTOP | WS_BORDER | Extra, X, Y, W, 24, Id);
	}

	void CreateControls()
	{
		constexpr int FieldX = 136;
		constexpr int FieldW = 356;
		constexpr int BrowseW = 84;
		constexpr int PathW = FieldW - BrowseW - 8;

		Label(L"Map folder", 16);
		Edit(IdFolderEdit, FieldX, 16, PathW);
		Make(L"BUTTON", L"Browse...", WS_TABSTOP | BS_PUSHBUTTON, FieldX + PathW + 8, 15, BrowseW, 26, IdFolderBrowse);
		Make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, FieldX, 44, FieldW, 32, IdFolderStatus);

		Label(L"Preview image", 80);
		Edit(IdImageEdit, FieldX, 80, PathW);
		Make(L"BUTTON", L"Browse...", WS_TABSTOP | BS_PUSHBUTTON, FieldX + PathW + 8, 79, BrowseW, 26, IdImageBrowse);
		Make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, FieldX, 108, FieldW, 32, IdImageStatus);

		Label(L"Title", 144);
		Edit(IdTitleEdit, FieldX, 144, FieldW);
		SendMessageW(Control(IdTitleEdit), EM_SETLIMITTEXT, MaxTitleChars, 0);

		Label(L"Description", 178);
		Make(L"EDIT", L"", WS_TABSTOP | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
			FieldX, 178, FieldW, 104, IdDescriptionEdit);
		SendMessageW(Control(IdDescriptionEdit), EM_SETLIMITTEXT, MaxDescriptionChars, 0);

		Label(L"Visibility", 292);
		Make(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, FieldX, 292, 240, 200, IdVisibilityCombo);
		for (const VisibilityOption& Option : VisibilityOptions)
		{
			SendMessageW(Control(IdVisibilityCombo), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Option.Label));
		}

		Label(L"Change note", 328);
		Edit(IdChangeNoteEdit, FieldX, 328, FieldW);

		Label(L"Workshop item ID", 364);
		Edit(IdItemIdEdit, FieldX, 364, 160, ES_AUTOHSCROLL | ES_NUMBER);
		Make(L"STATIC", L"Leave blank to create a new item. Keep the ID to update that item.", SS_LEFT, FieldX, 392, FieldW, 20, -1);

		// The preview itself is drawn by WM_PAINT into this rectangle.
		constexpr int PreviewX = 512, PreviewY = 16, PreviewW = 288, PreviewH = 162;
		G.PreviewRect = { Scale(PreviewX), Scale(PreviewY), Scale(PreviewX + PreviewW), Scale(PreviewY + PreviewH) };
		Make(L"STATIC", L"Shown on the item's Workshop page and in Workshop search.",
			SS_LEFT, PreviewX, PreviewY + PreviewH + 8, PreviewW, 40, -1);

		Make(PROGRESS_CLASSW, L"", PBS_SMOOTH, 16, 428, 784, 16, IdProgress);
		SendMessageW(Control(IdProgress), PBM_SETRANGE32, 0, 1000);
		Make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 16, 454, 560, 40, IdStatus);
		Make(L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON, 584, 454, 112, 30, IdOpenPage);
		ShowWindow(Control(IdOpenPage), SW_HIDE);
		Make(L"BUTTON", L"Upload", WS_TABSTOP | BS_DEFPUSHBUTTON, 704, 454, 96, 30, IdUpload);

		EnumChildWindows(G.Window, [](HWND Child, LPARAM Font) -> BOOL {
			SendMessageW(Child, WM_SETFONT, static_cast<WPARAM>(Font), TRUE);
			return TRUE;
		}, reinterpret_cast<LPARAM>(G.Font));

		SetWindowTextW(Control(IdFolderEdit), ReadSetting(L"ContentFolder").c_str());
		SetWindowTextW(Control(IdImageEdit), ReadSetting(L"PreviewImage").c_str());
		SetWindowTextW(Control(IdTitleEdit), ReadSetting(L"Title").c_str());
		SetWindowTextW(Control(IdDescriptionEdit), ReadSetting(L"Description").c_str());
		SetWindowTextW(Control(IdItemIdEdit), ReadSetting(L"ItemId").c_str());
		const std::wstring Visibility = ReadSetting(L"Visibility");
		SendMessageW(Control(IdVisibilityCombo), CB_SETCURSEL, Visibility.empty() ? 0 : _wtoi(Visibility.c_str()), 0);

		RefreshFolder();
		RefreshImage();
		SetStatus(L"Fill in the details and press Upload. Steam must be running.");
	}

	void PaintPreview(HDC Dc)
	{
		const RECT& R = G.PreviewRect;
		FillRect(Dc, &R, GetSysColorBrush(COLOR_3DDKSHADOW));

		if (!G.Preview)
		{
			SetBkMode(Dc, TRANSPARENT);
			SetTextColor(Dc, RGB(200, 200, 200));
			const HGDIOBJ Old = SelectObject(Dc, G.Font);
			RECT Text = R;
			DrawTextW(Dc, L"No preview image", -1, &Text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			SelectObject(Dc, Old);
			return;
		}

		// Letterboxed so the whole image shows.
		const float BoxW = static_cast<float>(R.right - R.left);
		const float BoxH = static_cast<float>(R.bottom - R.top);
		const float Fit = std::min(BoxW / G.Preview->GetWidth(), BoxH / G.Preview->GetHeight());
		const float W = G.Preview->GetWidth() * Fit;
		const float H = G.Preview->GetHeight() * Fit;

		Gdiplus::Graphics Graphics(Dc);
		Graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		Graphics.DrawImage(G.Preview.get(), R.left + (BoxW - W) / 2, R.top + (BoxH - H) / 2, W, H);
	}

	LRESULT CALLBACK WindowProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
	{
		switch (Message)
		{
		case WM_CREATE:
			G.Window = Hwnd;
			CreateControls();
			return 0;

		case WM_COMMAND:
			switch (LOWORD(WParam))
			{
			case IdFolderBrowse:
				if (const std::wstring Path = PickPath(true, Trim(GetText(IdFolderEdit))); !Path.empty())
				{
					SetWindowTextW(Control(IdFolderEdit), Path.c_str());
				}
				return 0;
			case IdImageBrowse:
				if (const std::wstring Path = PickPath(false, Trim(GetText(IdImageEdit))); !Path.empty())
				{
					SetWindowTextW(Control(IdImageEdit), Path.c_str());
				}
				return 0;
			case IdFolderEdit:
				if (HIWORD(WParam) == EN_CHANGE)
				{
					RefreshFolder();
				}
				return 0;
			case IdImageEdit:
				if (HIWORD(WParam) == EN_CHANGE)
				{
					RefreshImage();
				}
				return 0;
			case IdUpload:
				StartUpload();
				return 0;
			case IdOpenPage:
				ShellExecuteW(Hwnd, L"open", G.PageUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				return 0;
			}
			break;

		case WM_TIMER:
			if (WParam == SteamTimer)
			{
				PollSteam();
			}
			return 0;

		case WM_CTLCOLORSTATIC:
		{
			const HDC Dc = reinterpret_cast<HDC>(WParam);
			const int Id = GetDlgCtrlID(reinterpret_cast<HWND>(LParam));
			SetBkColor(Dc, GetSysColor(COLOR_BTNFACE));
			if (Id == IdFolderStatus)
			{
				SetTextColor(Dc, G.FolderColor);
			}
			else if (Id == IdImageStatus)
			{
				SetTextColor(Dc, G.ImageColor);
			}
			else if (Id == IdStatus)
			{
				SetTextColor(Dc, G.StatusColor);
			}
			return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
		}

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			const HDC Dc = BeginPaint(Hwnd, &Paint);
			PaintPreview(Dc);
			EndPaint(Hwnd, &Paint);
			return 0;
		}

		case WM_CLOSE:
			if (G.State != UploadState::Idle
				&& MessageBoxW(Hwnd, L"An upload is still running. Close anyway?", L"HAUNT Workshop Uploader",
					   MB_YESNO | MB_ICONWARNING) != IDYES)
			{
				return 0;
			}
			SaveSettings();
			DestroyWindow(Hwnd);
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(Hwnd, Message, WParam, LParam);
	}
}

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, PWSTR, int ShowCommand)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
	G.Dpi = GetDpiForSystem();

	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	INITCOMMONCONTROLSEX Controls{ sizeof(Controls), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
	InitCommonControlsEx(&Controls);

	ULONG_PTR GdiplusToken = 0;
	Gdiplus::GdiplusStartupInput GdiplusInput;
	Gdiplus::GdiplusStartup(&GdiplusToken, &GdiplusInput, nullptr);

	NONCLIENTMETRICSW Metrics{ sizeof(Metrics) };
	SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(Metrics), &Metrics, 0, G.Dpi);
	G.Font = CreateFontIndirectW(&Metrics.lfMessageFont);

	WNDCLASSEXW Class{ sizeof(Class) };
	Class.lpfnWndProc = WindowProc;
	Class.hInstance = Instance;
	Class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	Class.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	Class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
	Class.lpszClassName = L"HauntWorkshopUploader";
	RegisterClassExW(&Class);

	constexpr DWORD Style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	RECT Frame{ 0, 0, Scale(816), Scale(504) };
	AdjustWindowRectExForDpi(&Frame, Style, FALSE, 0, G.Dpi);

	const HWND Window = CreateWindowExW(0, Class.lpszClassName, L"HAUNT Workshop Uploader", Style,
		CW_USEDEFAULT, CW_USEDEFAULT, Frame.right - Frame.left, Frame.bottom - Frame.top,
		nullptr, nullptr, Instance, nullptr);
	ShowWindow(Window, ShowCommand);

	MSG Message;
	while (GetMessageW(&Message, nullptr, 0, 0) > 0)
	{
		if (!IsDialogMessageW(Window, &Message))
		{
			TranslateMessage(&Message);
			DispatchMessageW(&Message);
		}
	}

	if (G.bSteamReady)
	{
		SteamAPI_Shutdown();
	}
	G.Preview.reset();
	Gdiplus::GdiplusShutdown(GdiplusToken);
	DeleteObject(G.Font);
	CoUninitialize();
	return 0;
}
