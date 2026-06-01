/*
*
* Copyright 2016 Activision Publishing, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#pragma once

class mlBuildThread : public QThread
{
	Q_OBJECT

public:
	mlBuildThread(const QList<QPair<QString, QStringList>>& Commands, bool IgnoreErrors);
	void run();
	bool Succeeded() const
	{
		return mSuccess;
	}

	void Cancel()
	{
		mCancel = true;
		if (mActiveProcessId > 0)
			QProcess::execute("taskkill", QStringList() << "/PID" << QString::number(mActiveProcessId) << "/T" << "/F");
	}

signals:
	void OutputReady(const QString& Output);
	void CommandFinished(const QString& Program, const QStringList& Arguments, qint64 DurationMs, int ExitCode);

protected:
	QList<QPair<QString, QStringList>> mCommands;
	bool mSuccess;
	bool mCancel;
	bool mIgnoreErrors;
	volatile qint64 mActiveProcessId;
};

class mlConvertThread : public QThread
{
	Q_OBJECT

public:
	mlConvertThread(QStringList& Files, QString& OutputDir, bool IgnoreErrors, bool mOverwrite);
	void run();
	bool Succeeded() const
	{
		return mSuccess;
	}

	void Cancel()
	{
		mCancel = true;
		if (mActiveProcessId > 0)
			QProcess::execute("taskkill", QStringList() << "/PID" << QString::number(mActiveProcessId) << "/T" << "/F");
	}

signals:
	void OutputReady(const QString& Output);

protected:
	QStringList mFiles;
	QString mOutputDir;
	bool mOverwrite;

	bool mSuccess;
	bool mCancel;
	bool mIgnoreErrors;
	volatile qint64 mActiveProcessId;
};

class QuickLaunchPicker;

class mlMainWindow : public QMainWindow
{
	Q_OBJECT

	friend class Export2BinGroupBox;

public:
	mlMainWindow();
	~mlMainWindow();

	void UpdateDB();

	void OnCreateItemResult(CreateItemResult_t* CreateItemResult, bool IOFailure);
	CCallResult<mlMainWindow, CreateItemResult_t> mSteamCallResultCreateItem;

	void OnUpdateItemResult(SubmitItemUpdateResult_t* UpdateItemResult, bool IOFailure);
	CCallResult<mlMainWindow, SubmitItemUpdateResult_t> mSteamCallResultUpdateItem;

	void OnUGCRequestUGCDetails(SteamUGCRequestUGCDetailsResult_t* RequestDetailsResult, bool IOFailure);
	CCallResult<mlMainWindow, SteamUGCRequestUGCDetailsResult_t> mSteamCallResultRequestDetails;

protected slots:
	void OnFileNew();
	void OnFileAssetEditor();
	void OnFileLevelEditor();
	void OnFileExport2Bin();
	void OnEditBuild();
	void OnEditBuildAllLanguages();
	void OnEditReadyForPublish();
	void OnAnalyzeItem();
	void OnEditPublish();
	void OnEditOptions();
	void OnSetModernTheme();
	void OnSetDarkModernTheme();
	void OnSetClassicTheme();
	void OnEditDvars();
	void OnHelpAbout();
	void OnHelpCredits();
	void OnHelpGuide();
	void OnCheckForUpdates();
	void OnOpenZoneFile();
	void OnOpenModRootFolder();
	void OnOpenConsoleLog();
	void OnOpenConsoleLogExternal();
	void OnOpenScriptReference();
	void OnRunMapOrMod();
	void OnSaveLog() const;
	void OnCleanXPaks();
	void OnDelete();
	void OnRenameItem();
	void OnToggleFavorite();
	void OnExport2BinChooseDirectory();
	void OnExport2BinToggleOverwriteFiles();
	void BuildOutputReady(QString Output);
	void FlushBuildOutput();
	void BuildFinished();
	void ContextMenuRequested(const QPoint& Position);
	void SteamUpdate();
	void StatsTick();

protected:
	void closeEvent(QCloseEvent* Event);
	void showEvent(QShowEvent* Event);
	bool eventFilter(QObject* Watched, QEvent* Event);

	enum BuildLanguageMode
	{
		BuildAllLanguages,
		BuildEnglishOnly
	};

	enum GameRunningState
	{
		GameNotRunning,
		GameStarting,
		GameRunning
	};

	enum CategoryTab
	{
		CategoryAll,
		CategoryRecent,
		CategoryFavorites,
		CategoryZMMaps,
		CategoryMPMaps,
		CategoryMods
	};

	void StartBuild(BuildLanguageMode Mode);
	void StartBuildThread(const QList<QPair<QString, QStringList>>& Commands);
	void StartConvertThread(QStringList& pathList, QString& outputDir, bool allowOverwrite);
	void SetActiveBuildButton(BuildLanguageMode Mode);
	void ResetBuildButtons();
	QTreeWidgetItem* ActiveFileItem() const;
	void CheckForUpdates(bool UserInitiated);
	void HandleUpdateMetadata(const QJsonObject& Root, bool UserInitiated);
	void RecordUserActivity();
	void UpdateStatsTimers();
	void FlushCurrentItemTime();
	void SetCurrentStatsItem(QTreeWidgetItem* Item);
	QString NormalizeStatsItemKey(const QString& FavoriteKey) const;
	void AddStatSeconds(const QString& Key, qint64 Seconds);
	void IncrementStat(const QString& Key, qint64 Amount = 1);
	qint64 StatValue(const QString& Key) const;
	QString FormatDuration(qint64 Seconds) const;
	QString DisplayNameForStatsKey(const QString& FavoriteKey) const;
	void TrackProcessLifetime(QProcess* Process, const QString& CountKey, const QString& SecondsKey, const QString& PerItemSecondsPrefix = QString(), const QString& ItemKey = QString());
	void OnBuildCommandFinished(const QString& Program, const QStringList& Arguments, qint64 DurationMs, int ExitCode);
	void SetFooterUpdateState(const QString& Message, const QString& DownloadUrl = QString(), const QString& VersionLabel = QString(), const QString& ReleasePageUrl = QString(), bool UpdateAvailable = false);
	QString GetItemContainerName(QTreeWidgetItem* Item) const;
	QString GetItemEntryName(QTreeWidgetItem* Item) const;
	QString GetItemFavoriteKey(QTreeWidgetItem* Item) const;
	QStringList FavoriteEntries() const;
	QStringList RecentEntries() const;
	bool IsFavoriteEntry(const QString& Entry) const;
	void ToggleFavoriteEntry(const QString& Entry);
	void TouchRecentEntry(const QString& Entry);
	QString RecentEntryForItem(int ItemType, const QString& ContainerName, const QString& EntryName) const;
	QStringList ModZoneNames(const QString& ModName) const;
	Qt::CheckState CheckStateForKey(const QString& FavoriteKey) const;
	void ApplyCheckStateForKey(const QString& FavoriteKey, Qt::CheckState State, QTreeWidgetItem* SkipItem = NULL);
	void SyncItemCheckWidget(QTreeWidgetItem* Item) const;
	void SyncItemSelectionWidget(QTreeWidgetItem* Item) const;
	void UpdateParentCheckState(QTreeWidgetItem* Item);
	void SetTreeItemChecked(QTreeWidgetItem* Item, Qt::CheckState State, bool PropagateChildren);
	bool SupportsDisplayName(int ItemType) const;
	QString DisplayNameForEntry(const QString& FavoriteKey) const;
	void SetDisplayNameForEntry(const QString& FavoriteKey, const QString& DisplayName);
	QString DisplayColorForEntry(const QString& FavoriteKey) const;
	void SetDisplayColorForEntry(const QString& FavoriteKey, const QString& ColorValue);
	bool PromptForDisplayName(int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey);
	QList<QTreeWidgetItem*> CollectTargetItems(bool* HasMapSelection = NULL) const;
	bool MapMatchesCurrentTab(const QString& MapName) const;
	GameRunningState DetectGameRunningState();
	bool IsTrackedGameProcessAlive() const;
	QString DisplayTextForItem(int ItemType, const QString& ContainerName, const QString& EntryName) const;
	QString ItemContentRoot(int ItemType, const QString& ContainerName, const QString& EntryName) const;
	quint64 ItemDiskSizeBytes(int ItemType, const QString& ContainerName, const QString& EntryName) const;
	QWidget* CreateItemTitleWidget(QTreeWidgetItem* Item, int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey, const QString& BaseText);
	void PopulatePinnedRoot(QTreeWidgetItem* RootItem, const QStringList& Keys, const QHash<QString, QVariantMap>& Lookup);
	QWidget* CreateQuickActionsWidget(QTreeWidgetItem* Item, int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey, bool IsChildRow);
	void RunQuickAction(int ItemType, const QString& ContainerName, const QString& EntryName, bool LinkAction, bool RunAction);
	void QueueGameStatsForItem(const QString& FavoriteKey);
	void FinalizeGameStatsTransition(GameRunningState PreviousState, GameRunningState CurrentState);
	void ShowItemInformationDialog(QTreeWidgetItem* Item);
	bool ShouldUseSteamOnlineLaunch() const;
	QString ResolveSteamExecutablePath() const;
	QPair<QString, QStringList> CreateGameLaunchCommand(const QString& FsGame, const QString& MapName = QString()) const;
	QStringList DetectBuiltLanguages(const QString& ContentRoot) const;
	bool HasOnlyEnglishBuild(const QString& ContentRoot) const;
	QString SectionSettingKey(const QString& SectionName) const;
	QString LauncherDataRoot() const;
	QString DefaultScriptsManifestPath() const;
	QString DefaultUiManifestPath() const;
	QString NotesFilePath(int ItemType, const QString& ContainerName, const QString& EntryName) const;
	QString WorkshopVersionsRoot(int ItemType, const QString& ContainerName, const QString& EntryName) const;
	QString ActiveWorkshopFolder(int ItemType, const QString& ContainerName, const QString& EntryName) const;
	bool EditNotesForItem(QTreeWidgetItem* Item);
	bool SaveWorkshopVersionSnapshot(int ItemType, const QString& ContainerName, const QString& EntryName, const QString& SourceWorkshopJsonPath, const QString& VersionLabel, const QString& OverridePublisherId = QString());
	bool ActivateWorkshopVersion(int ItemType, const QString& ContainerName, const QString& EntryName, const QString& VersionFolderPath);
	void ShowWorkshopVersionsDialog(QTreeWidgetItem* Item);
	bool EnsureDefaultScriptsManifest(QString* Error = NULL) const;
	bool EnsureDefaultUiManifest(QString* Error = NULL) const;
	QSet<QString> LoadDefaultScriptsManifest(QString* Error = NULL) const;
	QSet<QString> LoadDefaultUiManifest(QString* Error = NULL) const;
	bool RenameSelectedItem(QTreeWidgetItem* Item, const QString& NewName, bool DuplicateMode);
	void ShowAnalysisResults(const QString& Title, const QStringList& Findings, const QStringList& Warnings) const;

	void PopulateFileList();
	bool SaveWorkshopMetadata();
	void UpdateWorkshopItem(bool UploadToWorkshop = true);
	void ShowPublishDialog();
	void UpdateTheme();
	void UpdateBackgroundOverlays();
	void UpdateThemeMenuChecks();
	void EnsureThemeProfiles();
	QString CurrentThemeProfileId() const;
	QStringList AvailableThemeProfileIds() const;
	QString ThemeProfileDisplayName(const QString& ThemeProfileId) const;
	QVariantMap ThemeProfileValues(const QString& ThemeProfileId) const;
	void SaveThemeProfile(const QString& ThemeProfileId, const QString& DisplayName, const QVariantMap& Values);
	void ApplyThemeProfile(const QString& ThemeProfileId);
	void PopulateQuickLaunchEntries();
	void PopulateWorkshopQuickLaunchEntries();
	void ShowOnlineLaunchProgressDialog(const QString& TargetLabel);
	void CloseOnlineLaunchProgressDialog();
	void UpdateBuildActionButtons();
	void UpdateQuickLaunchVisibility();
	void UpdateCategorySummary(const QHash<QString, QVariantMap>& Lookup, const QStringList& Recents, const QStringList& Favorites);
	void UpdateOutputConsoleMode();
	void RebuildOutputFromBuffer();
	void AppendOutputBlock(const QString& BlockText, const QSettings& Settings);

	void RebuildCategoryTabs();
	void ShowCategoryTabEditDialog(int TabIndex);
	bool IsCustomTab(int TabIndex) const;
	void RefreshRunDvars();
	bool IsNewerLauncherVersion(const QString& AvailableVersion) const;
	QString UpdateApiUrl() const;
	QString UpdateReleasesPageUrl() const;
	QJsonObject SelectPrimaryReleaseAsset(const QJsonArray& Assets) const;
	void StartUpdateDownload(const QUrl& DownloadUrl, const QString& VersionLabel);
	bool LaunchUpdateInstaller(const QString& ZipPath, const QString& VersionLabel);
	bool IsGameRunning() const;
	void UpdateGameRunningState();
	void ApplyLauncherLayout();

	void CreateActions();
	void CreateMenu();
	void CreateToolBar();

	void InitExport2BinGUI();

	QAction* mActionFileNew;
	QAction* mActionFileAssetEditor;
	QAction* mActionFileOpenRoot;
	QAction* mActionFileOpenConsoleLog;
	QAction* mActionFileOpenConsoleLogExternal;
	QAction* mActionFileOpenScriptReference;
	QAction* mActionFileLevelEditor;
	QAction* mActionFileExport2Bin;
	QAction* mActionFileExit;
	QAction* mActionEditBuild;
	QAction* mActionEditBuildAllLanguages;
	QAction* mActionEditAnalyze;
	QAction* mActionEditInformation;
	QAction* mActionEditReadyForPublish;
	QAction* mActionEditPublish;
	QAction* mActionEditOptions;
	QAction* mActionHelpAbout;
	QAction* mActionHelpCredits;
	QAction* mActionHelpCheckUpdates;
	QAction* mActionHelpGuide;
	QAction* mActionThemeModern;
	QAction* mActionThemeDarkModern;
	QAction* mActionThemeClassic;

	QTabBar* mCategoryTabs;
	QToolButton* mCategoryTabPencilButton;
	QPushButton* mAddCategoryTabButton;
	int mCategoryTabHoveredIndex;
	QLabel* mCategorySummaryLabel;
	QSplitter* mCentralWidgetSplitter;
	QDockWidget* mAssetDockWidget;
	QDockWidget* mOutputDockWidget;
	QToolBar* mMainToolBar;
	QWidget* mTopWidget;
	QWidget* mLeftPanel;
	QWidget* mActionsPanel;
	QWidget* mOutputPanel;
	QHBoxLayout* mTopLayout;
	QTreeWidget* mFileListWidget;
	QTabBar* mOutputTabs;
	QTreeWidget* mOutputWidget;
	QPlainTextEdit* mOutputPlainWidget;
	QLabel* mAssetTreeBackgroundOverlay;
	QLabel* mOutputBackgroundOverlay;
	QToolButton* mLogFiltersButton;
	QToolButton* mLogSelectionButton;
	QLabel* mFooterVersionLabel;
	QToolButton* mFooterRefreshButton;
	QLabel* mFooterUpdateStatusLabel;
	QPushButton* mFooterDownloadButton;
	QTreeWidgetItem* mCurrentOutputBlockItem;
	int mOutputBlockCounter;

	QPushButton* mBuildButton;
	QPushButton* mBuildAllLanguagesButton;
	QPushButton* mAnalyzeItemButton;
	QPushButton* mActiveBuildButton;
	QPushButton* mThemesButton;
	QPushButton* mDvarsButton;
	QPushButton* mLogButton;
	QCheckBox* mCompileEnabledWidget;
	QComboBox* mCompileModeWidget;
	QCheckBox* mLightEnabledWidget;
	QComboBox* mLightQualityWidget;
	QCheckBox* mLinkEnabledWidget;
	QCheckBox* mRunEnabledWidget;
	QCheckBox* mRunOnlineWidget;
	QLineEdit* mRunOptionsWidget;
	QLabel* mQuickLaunchLabel;
	QLabel* mStartupQuoteLabel;
	QString mStartupQuoteText;
	bool mStartupQuotePopupShown;
	QuickLaunchPicker* mQuickLaunchWidget;
	QPushButton* mCloseGameButton;
	QLabel* mCloseGameStatusLabel;
	QCheckBox* mIgnoreErrorsWidget;

	mlBuildThread* mBuildThread;
	mlConvertThread* mConvertThread;

	QDockWidget* mExport2BinGUIWidget;
	QCheckBox* mExport2BinOverwriteWidget;
	QLineEdit* mExport2BinTargetDirWidget;
	QPointer<QProgressDialog> mLaunchProgressDialog;
	QNetworkAccessManager* mUpdateNetworkAccess;
	QPointer<QNetworkReply> mUpdateMetadataReply;
	QPointer<QNetworkReply> mUpdateDownloadReply;
	QPointer<QProgressDialog> mUpdateProgressDialog;
	QFile* mUpdateDownloadFile;
	QString mUpdateDownloadPath;
	QString mPendingUpdateVersion;
	QString mAvailableUpdateVersion;
	QString mAvailableUpdateUrl;
	QString mAvailableReleasePageUrl;
	QTimer mStatsTimer;
	QElapsedTimer mStatsElapsedTimer;
	qint64 mLastStatsTickMs;
	qint64 mLastActivityMs;
	qint64 mCurrentItemStartedMs;
	qint64 mPendingGameQueuedMs;
	qint64 mCurrentGameStartedMs;
	qint64 mPendingLaunchRequestedMs;
	QString mCurrentStatsItemKey;
	QString mPendingGameStatsItemKey;
	QString mActiveGameStatsItemKey;

	int mThemeMode;
	QString mThemeProfileId;
	bool mCachedGameRunning;
	quint32 mGameProcessId;
	GameRunningState mGameRunningState;
	QString mBuildLanguage;
	QString mLauncherLayout;
	QHash<QString, Qt::CheckState> mCheckedStateByKey;
	QString mAssetTreeBackgroundCachePath;
	QString mLogBackgroundCachePath;

	QStringList mShippedMapList;
	QTimer mTimer;
	QTimer mOutputFlushTimer;

	quint64 mFileId;
	QString mTitle;
	QString mBriefingDescription;
	QString mSteamDescription;
	QString mThumbnail;
	QString mWorkshopFolder;
	QString mFolderName;
	QString mType;
	QStringList mTags;
	bool mWorkshopUploadInFlight;
	bool mPostUploadSteamSyncPending;

	QString mGamePath;
	QString mToolsPath;

	QString mPendingOutput;
	QString mOutputFullText;
	bool mOutputSelectionMode;
	int mOutputTabIndex;
	bool mOutputTreeAutoFollow;
	bool mOutputPlainAutoFollow;
	bool mPendingOnlineLaunchFeedback;
	QString mPendingOnlineLaunchLabel;

	QStringList mRunDvars;
};

class Export2BinGroupBox : public QGroupBox
{
private:
	mlMainWindow* parentWindow;

protected:
	void dragEnterEvent(QDragEnterEvent* event);
	void dragLeaveEvent(QDragLeaveEvent* event);
	void dropEvent(QDropEvent *event);

public:
	Export2BinGroupBox(QWidget *parent, mlMainWindow* parent_window);
};
