/*
*
* Copyright 2016 Activision Publishing, Inc.
*
			UpdateBuildActionButtons();
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

#include "stdafx.h"

#include "mlMainWindow.h"

#include <functional>
#include <iomanip>
#include <sstream>
#include <TlHelp32.h>
#include <utility>

#pragma comment(lib, "steam_api64.lib")

const int AppId = 311210;
const char* gLanguages[] = { "english", "french", "italian", "spanish", "german", "portuguese", "russian", "polish", "japanese", "traditionalchinese", "simplifiedchinese", "englisharabic" };
const char* gTags[] = { "Animation", "Audio", "Character", "Map", "Mod", "Mode", "Model", "Multiplayer", "Scorestreak", "Skin", "Specialist", "Texture", "UI", "Vehicle", "Visual Effect", "Weapon", "WIP", "Zombies" };
dvar_s gDvars[] = {
					{"ai_disableSpawn", "Disable AI from spawning", DVAR_VALUE_BOOL},
					{"developer", "Run developer mode", DVAR_VALUE_INT, 0, 2},
					{"g_password", "Password for your server", DVAR_VALUE_STRING},
					{"logfile", "Console log information written to current fs_game", DVAR_VALUE_INT, 0, 2},
					{"scr_mod_enable_devblock", "Developer blocks are executed in mods ", DVAR_VALUE_BOOL},
					{"connect", "Connect to a specific server", DVAR_VALUE_STRING, NULL, NULL, true},
					{"set_gametype", "Set a gametype to load with map", DVAR_VALUE_STRING, NULL, NULL, true},
					{"splitscreen", "Enable splitscreen", DVAR_VALUE_BOOL},
					{"splitscreen_playerCount", "Allocate the number of instances for splitscreen", DVAR_VALUE_INT, 0, 2}
				 };
static const char* kLauncherVersion = "1.0.4";
// Point these at the repo that publishes the launcher ZIP asset users should install.
static const char* kLauncherReleaseApiUrl = "https://api.github.com/repos/Sphynxmods/BO3-Mod-Tools-Black/releases/latest";
static const char* kLauncherReleasesUrl = "https://github.com/Sphynxmods/BO3-Mod-Tools-Black/releases";

static QString CleanVersionText(QString VersionText)
{
	VersionText = VersionText.trimmed();
	if (VersionText.startsWith('v', Qt::CaseInsensitive))
		VersionText.remove(0, 1);
	return VersionText;
}

static QVersionNumber VersionNumberFromText(const QString& VersionText)
{
	return QVersionNumber::fromString(CleanVersionText(VersionText));
}

enum ThemeModeValue
{
	ThemeOriginalUpdated,
	ThemeOriginalClassic,
	ThemeDarkModern
};

static QStringList StartupQuotes()
{
	return QStringList()
		<< "Today, you're going to achieve greatness. Greatness starts with something small."
		<< "You can do it. One step at a time."
		<< "Small progress is still progress."
		<< "Start small. Finish strong."
		<< "Momentum beats hesitation."
		<< "Build the next piece. Then the next one."
		<< "Today is a good day to make something better.";
}

static QString RandomStartupQuote()
{
	const QStringList Quotes = StartupQuotes();
	if (Quotes.isEmpty())
		return QString();
	return Quotes.at(QRandomGenerator::global()->bounded(Quotes.count()));
}

namespace
{
struct ToolbarItemConfig
{
	QString Key;
	QString Label;
	QString IconPath;
	QString BuiltInActionKey;
	QString FunctionScript;
	bool BuiltIn;
	bool Hidden;
};

struct ToolbarBuiltinSpec
{
	const char* Key;
	const char* Label;
	bool HiddenByDefault;
};

static const ToolbarBuiltinSpec kToolbarBuiltinSpecs[] =
{
	{ "file-new", "New", false },
	{ "file-open-root", "Open Root", true },
	{ "file-open-console-log", "Open console_mp.log", true },
	{ "file-open-console-log-editor", "Open console_mp.log (Editor)", true },
	{ "help-script-reference", "BO3 Script Reference", true },
	{ "edit-build", "Build", false },
	{ "edit-analyze", "Analyze", false },
	{ "edit-information", "Information", false },
	{ "edit-ready-for-publish", "Publish Check", false },
	{ "edit-publish", "Publish", false },
	{ "file-asset-editor", "Asset Editor", false },
	{ "file-level-editor", "Radiant", false },
	{ "file-export2bin", "Export2Bin", true },
	{ "extra-tools-menu", "Extra Tools", false }
};

static const int kToolbarSchemaVersion = 3;

static QString BuiltInActionKeyForSlot(const QString& Key)
{
	QString NormalizedKey = Key.trimmed().toLower();
	if (NormalizedKey == "plugins-menu")
		NormalizedKey = "extra-tools-menu";
	return NormalizedKey;
}

static QList<ToolbarItemConfig> DefaultToolbarItems()
{
	QList<ToolbarItemConfig> Items;
	const QStringList DefaultOrder = QStringList()
		<< "file-new"
		<< "edit-build"
		<< "edit-analyze"
		<< "edit-information"
		<< "edit-ready-for-publish"
		<< "edit-publish"
		<< "file-asset-editor"
		<< "file-level-editor"
		<< "extra-tools-menu";

	for (const QString& BuiltinKey : DefaultOrder)
	{
		const ToolbarBuiltinSpec* SpecPtr = NULL;
		for (const ToolbarBuiltinSpec& Candidate : kToolbarBuiltinSpecs)
		{
			if (QString::fromLatin1(Candidate.Key) == BuiltinKey)
			{
				SpecPtr = &Candidate;
				break;
			}
		}
		if (!SpecPtr)
			continue;

		ToolbarItemConfig Item;
		Item.Key = BuiltInActionKeyForSlot(SpecPtr->Key);
		Item.Label = QString::fromLatin1(SpecPtr->Label);
		Item.BuiltInActionKey = Item.Key;
		Item.BuiltIn = true;
		Item.Hidden = SpecPtr->HiddenByDefault;
		Items.append(Item);
	}
	return Items;
}

static QString ToolbarIconResourcePath(const QString& BuiltInActionKey, bool LegacyIcons)
{
	const QString Key = BuiltInActionKey.trimmed().toLower();
	if (Key == "file-new")
		return LegacyIcons ? ":/resources/FileNew.png" : ":/resources/new-mapmod.png";
	if (Key == "edit-build")
		return LegacyIcons ? ":/resources/Go.png" : ":/resources/build.png";
	if (Key == "edit-analyze")
		return LegacyIcons ? ":/resources/Go.png" : ":/resources/diagnosis.png";
	if (Key == "edit-ready-for-publish")
		return LegacyIcons ? ":/resources/upload.png" : ":/resources/upload-check.png";
	if (Key == "edit-publish")
		return ":/resources/upload.png";
	if (Key == "file-asset-editor")
		return ":/resources/AssetEditor.png";
	if (Key == "file-level-editor")
		return ":/resources/Radiant.png";
	if (Key == "file-export2bin")
		return LegacyIcons ? ":/resources/Export2Bin.png" : ":/resources/Export2Bin.png";
	if (Key == "extra-tools-menu")
		return ":/resources/toolkit.png";
	if (Key == "file-open-root")
		return QCoreApplication::applicationDirPath() + "/folderIcon.png";
	if (Key == "help-script-reference")
		return ":/resources/Export2Bin.png";
	return QString();
}

static QIcon ToolbarIconForActionKey(const QString& BuiltInActionKey)
{
	const bool LegacyIcons = QSettings().value("UseLegacyToolbarIcons", false).toBool();
	const QString ResourcePath = ToolbarIconResourcePath(BuiltInActionKey, LegacyIcons);
	if (ResourcePath.isEmpty())
		return QIcon();
	if (ResourcePath.endsWith(".png", Qt::CaseInsensitive) || ResourcePath.endsWith(".ico", Qt::CaseInsensitive))
		return QIcon(ResourcePath);
	return QIcon(QDir::toNativeSeparators(ResourcePath));
}

static ToolbarItemConfig ToolbarItemFromMap(const QVariantMap& Map)
{
	ToolbarItemConfig Item;
	Item.Key = Map.value("Key").toString().trimmed();
	Item.Label = Map.value("Label").toString().trimmed();
	Item.IconPath = Map.value("IconPath").toString().trimmed();
	Item.BuiltInActionKey = BuiltInActionKeyForSlot(Map.value("BuiltInActionKey").toString());
	Item.FunctionScript = Map.value("FunctionScript").toString();
	Item.BuiltIn = Map.value("BuiltIn", false).toBool();
	Item.Hidden = Map.value("Hidden", false).toBool();
	if (Item.Key.isEmpty())
		Item.Key = Item.BuiltIn ? Item.BuiltInActionKey : Item.Label;
	if (Item.Key.compare("plugins-menu", Qt::CaseInsensitive) == 0)
		Item.Key = "extra-tools-menu";
	if (Item.Label.isEmpty())
		Item.Label = Item.BuiltIn ? Item.BuiltInActionKey : QObject::tr("Custom Item");
	if (Item.BuiltInActionKey.isEmpty() && Item.BuiltIn)
		Item.BuiltInActionKey = BuiltInActionKeyForSlot(Item.Key);
	if (Item.BuiltIn)
		Item.BuiltInActionKey = BuiltInActionKeyForSlot(Item.BuiltInActionKey);
	return Item;
}

static QVariantMap ToolbarItemToMap(const ToolbarItemConfig& Item)
{
	QVariantMap Map;
	Map.insert("Key", Item.Key);
	Map.insert("Label", Item.Label);
	Map.insert("IconPath", Item.IconPath);
	Map.insert("BuiltInActionKey", Item.BuiltInActionKey);
	Map.insert("FunctionScript", Item.FunctionScript);
	Map.insert("BuiltIn", Item.BuiltIn);
	Map.insert("Hidden", Item.Hidden);
	return Map;
}

static QList<ToolbarItemConfig> LoadToolbarItems()
{
	QSettings Settings;
	QList<ToolbarItemConfig> Items;
	Settings.beginGroup("Toolbar");
	const int SavedSchemaVersion = Settings.value("SchemaVersion", 0).toInt();
	if (SavedSchemaVersion < kToolbarSchemaVersion)
	{
		Settings.endGroup();
		return DefaultToolbarItems();
	}
	const int Count = Settings.beginReadArray("Items");
	for (int Index = 0; Index < Count; Index++)
	{
		Settings.setArrayIndex(Index);
		Items.append(ToolbarItemFromMap(Settings.value("Entry").toMap()));
	}
	Settings.endArray();
	Settings.endGroup();
	if (Items.isEmpty())
		Items = DefaultToolbarItems();

	bool HasExtraToolsMenu = false;
	for (ToolbarItemConfig& Item : Items)
	{
		if (!Item.BuiltIn)
			continue;
		Item.BuiltInActionKey = BuiltInActionKeyForSlot(Item.BuiltInActionKey);
		if (Item.Key.compare("plugins-menu", Qt::CaseInsensitive) == 0)
			Item.Key = "extra-tools-menu";
		if (Item.BuiltInActionKey == "extra-tools-menu")
		{
			HasExtraToolsMenu = true;
			if (Item.Label.trimmed().isEmpty() || Item.Label.compare("Plugins", Qt::CaseInsensitive) == 0)
				Item.Label = "Extra Tools";
		}
	}

	if (!HasExtraToolsMenu)
	{
		ToolbarItemConfig ExtraToolsItem;
		ExtraToolsItem.Key = "extra-tools-menu";
		ExtraToolsItem.Label = "Extra Tools";
		ExtraToolsItem.BuiltInActionKey = "extra-tools-menu";
		ExtraToolsItem.BuiltIn = true;
		ExtraToolsItem.Hidden = false;
		Items.append(ExtraToolsItem);
	}
	return Items;
}

static void SaveToolbarItems(const QList<ToolbarItemConfig>& Items)
{
	QSettings Settings;
	Settings.beginGroup("Toolbar");
	Settings.setValue("SchemaVersion", kToolbarSchemaVersion);
	Settings.beginWriteArray("Items");
	for (int Index = 0; Index < Items.count(); Index++)
	{
		Settings.setArrayIndex(Index);
		Settings.setValue("Entry", ToolbarItemToMap(Items[Index]));
	}
	Settings.endArray();
	Settings.endGroup();
}

static QAction* FindBuiltinToolbarAction(mlMainWindow* Window, const QString& BuiltInActionKey)
{
	const QString NormalizedKey = BuiltInActionKey.trimmed().toLower();
	return Window ? Window->findChild<QAction*>(NormalizedKey) : NULL;
}

static bool TriggerBuiltinToolbarAction(mlMainWindow* Window, const QString& BuiltInActionKey)
{
	QAction* Action = FindBuiltinToolbarAction(Window, BuiltInActionKey);
	if (!Action)
		return false;
	Action->trigger();
	return true;
}

static void OpenPathWithDefaultApp(const QString& Path)
{
	if (!Path.trimmed().isEmpty())
		QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::toNativeSeparators(Path)));
}

static bool ExecuteToolbarScript(mlMainWindow* Window, const QString& Script)
{
	QString CleanScript = Script;
	CleanScript.replace('\r', '\n');
	const QStringList Lines = CleanScript.split('\n', Qt::SkipEmptyParts);
	for (QString Line : Lines)
	{
		Line = Line.trimmed();
		if (Line.isEmpty() || Line.startsWith('#'))
			continue;

		const QString LowerLine = Line.toLower();
		if (LowerLine.startsWith("builtin:"))
		{
			const QString BuiltInKey = Line.mid(QStringLiteral("builtin:").length()).trimmed();
			if (!TriggerBuiltinToolbarAction(Window, BuiltInKey))
				QMessageBox::warning(Window, "Toolbar Item", QString("Unknown built-in toolbar command: %1").arg(BuiltInKey));
			continue;
		}

		if (LowerLine.startsWith("url:"))
		{
			const QString UrlText = Line.mid(QStringLiteral("url:").length()).trimmed();
			if (!UrlText.isEmpty())
				QDesktopServices::openUrl(QUrl(UrlText));
			continue;
		}

		if (LowerLine.startsWith("file:"))
		{
			OpenPathWithDefaultApp(Line.mid(QStringLiteral("file:").length()).trimmed());
			continue;
		}

		if (LowerLine.startsWith("folder:"))
		{
			OpenPathWithDefaultApp(Line.mid(QStringLiteral("folder:").length()).trimmed());
			continue;
		}

		if (LowerLine.startsWith("exe:"))
			Line = Line.mid(QStringLiteral("exe:").length()).trimmed();

		const QStringList CommandParts = QProcess::splitCommand(Line);
		if (CommandParts.isEmpty())
			continue;

		const QString Program = CommandParts[0];
		QStringList Arguments = CommandParts.mid(1);
		if (!QFileInfo(Program).exists() && !Program.contains('.') && !Line.contains(' '))
		{
			QMessageBox::warning(Window, "Toolbar Item", QString("Unable to launch '%1'.").arg(Program));
			continue;
		}

		if (!QProcess::startDetached(Program, Arguments))
		{
			QMessageBox::warning(Window, "Toolbar Item", QString("Unable to launch '%1'.").arg(Program));
			return false;
		}
	}

	return true;
}

static QString ResolveConsoleLogPath(const QString& GamePath, QTreeWidgetItem* ActiveItem)
{
	Q_UNUSED(ActiveItem);
	if (GamePath.trimmed().isEmpty())
		return QString();

	QStringList Candidates;
	Candidates << QDir::cleanPath(QString("%1/players/console_mp.log").arg(GamePath));
	Candidates << QDir::cleanPath(QString("%1/main/console_mp.log").arg(GamePath));
	Candidates << QDir::cleanPath(QString("%1/console_mp.log").arg(GamePath));

	for (const QString& Candidate : Candidates)
	{
		if (QFileInfo(Candidate).isFile())
			return Candidate;
	}

	return Candidates.isEmpty() ? QString() : Candidates.first();
}

static void ShowTextFileDialog(QWidget* Parent, const QString& FilePath, const QString& Title)
{
	QFile File(FilePath);
	if (!File.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(Parent, Title, QString("Unable to open:\n%1").arg(QDir::toNativeSeparators(FilePath)));
		return;
	}

	QDialog Dialog(Parent);
	Dialog.setWindowTitle(Title);
	Dialog.resize(980, 720);
	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QPlainTextEdit* Viewer = new QPlainTextEdit(&Dialog);
	Viewer->setReadOnly(true);
	Viewer->setLineWrapMode(QPlainTextEdit::NoWrap);
	Viewer->setPlainText(QString::fromUtf8(File.readAll()));
	Layout->addWidget(Viewer, 1);
	QDialogButtonBox* Buttons = new QDialogButtonBox(QDialogButtonBox::Close, &Dialog);
	QObject::connect(Buttons, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	Layout->addWidget(Buttons);
	Dialog.exec();
}

static QString NormalizeCategoryTabKey(const QString& RawKey)
{
	const QString Key = RawKey.trimmed().toLower();
	if (Key == "all")
		return "all";
	if (Key == "recent" || Key == "recents")
		return "recent";
	if (Key == "favorite" || Key == "favorites")
		return "favorites";
	if (Key == "zm" || Key == "zm maps" || Key == "zm_maps" || Key == "zmmaps" || Key == "zm-maps")
		return "zm-maps";
	if (Key == "mp" || Key == "mp maps" || Key == "mp_maps" || Key == "mpmaps" || Key == "mp-maps")
		return "mp-maps";
	if (Key == "mod" || Key == "mods")
		return "mods";
	return Key;
}

static bool IsAddCategoryTabKey(const QString& RawKey)
{
	return NormalizeCategoryTabKey(RawKey) == "__add-category-tab__";
}

static QString CurrentCategoryTabKey(const QTabBar* Tabs)
{
	if (!Tabs || Tabs->currentIndex() < 0)
		return "all";
	const QString DataKey = NormalizeCategoryTabKey(Tabs->tabData(Tabs->currentIndex()).toString());
	if (IsAddCategoryTabKey(DataKey))
		return "all";
	if (!DataKey.isEmpty())
		return DataKey;
	return NormalizeCategoryTabKey(Tabs->tabText(Tabs->currentIndex()));
}

static int FindCategoryTabIndex(const QTabBar* Tabs, const QString& CategoryKey)
{
	if (!Tabs)
		return -1;
	const QString NormalizedKey = NormalizeCategoryTabKey(CategoryKey);
	for (int TabIdx = 0; TabIdx < Tabs->count(); TabIdx++)
	{
		if (NormalizeCategoryTabKey(Tabs->tabData(TabIdx).toString()) == NormalizedKey)
			return TabIdx;
	}
	return -1;
}

// --- Custom tab persistence helpers ---

static QList<QVariantMap> LoadCustomTabDefs()
{
	QSettings Settings;
	const int Count = Settings.beginReadArray("CustomTabs");
	QList<QVariantMap> Result;
	for (int i = 0; i < Count; i++)
	{
		Settings.setArrayIndex(i);
		QVariantMap Tab;
		Tab["id"] = Settings.value("id").toString();
		Tab["name"] = Settings.value("name").toString();
		Tab["color"] = Settings.value("color").toString();
		Tab["icon"] = Settings.value("icon").toString();
		Tab["items"] = Settings.value("items", QStringList()).toStringList();
		if (!Tab["id"].toString().isEmpty() && !Tab["name"].toString().isEmpty())
			Result.append(Tab);
	}
	Settings.endArray();
	return Result;
}

static void SaveCustomTabDefs(const QList<QVariantMap>& Tabs)
{
	QSettings Settings;
	Settings.remove("CustomTabs");
	Settings.beginWriteArray("CustomTabs");
	for (int i = 0; i < Tabs.size(); i++)
	{
		Settings.setArrayIndex(i);
		const QVariantMap& Tab = Tabs[i];
		Settings.setValue("id", Tab.value("id").toString());
		Settings.setValue("name", Tab.value("name").toString());
		Settings.setValue("color", Tab.value("color").toString());
		Settings.setValue("icon", Tab.value("icon").toString());
		Settings.setValue("items", Tab.value("items").toStringList());
	}
	Settings.endArray();
}

static QString GenerateCustomTabId()
{
	return QString("custom-%1").arg(QDateTime::currentMSecsSinceEpoch());
}

static QString NormalizedTabIconPath(const QString& Value)
{
	QString Path = Value.trimmed();
	if (Path.startsWith("qrc:/"))
		Path = ":" + Path.mid(4);
	return Path;
}

static QIcon BuildTabIcon(const QString& Value)
{
	const QString Path = NormalizedTabIconPath(Value);
	if (Path.isEmpty())
		return QIcon();

	if (Path.startsWith(":"))
	{
		const QPixmap Pix(Path);
		return Pix.isNull() ? QIcon() : QIcon(Pix);
	}

	const QFileInfo Info(Path);
	if (!Info.isFile())
		return QIcon();

	const QPixmap Pix(Path);
	return Pix.isNull() ? QIcon() : QIcon(Pix);
}

static QString BuildTabLabel(const QString& Icon, const QString& Name)
{
	if (!BuildTabIcon(Icon).isNull())
		return Name;
	const QString TrimmedIcon = Icon.trimmed();
	if (TrimmedIcon.isEmpty())
		return Name;
	return TrimmedIcon + "  " + Name;
}

static QStringList GetCustomTabItems(const QString& TabKey)
{
	const QList<QVariantMap> Tabs = LoadCustomTabDefs();
	for (const QVariantMap& Tab : Tabs)
	{
		if (Tab.value("id").toString() == TabKey)
			return Tab.value("items").toStringList();
	}
	return {};
}
}

static QString ThemeModeToSettingsValue(int ThemeModeValue)
{
	switch (ThemeModeValue)
	{
	case ThemeOriginalClassic:
		return "original-classic";
	case ThemeDarkModern:
		return "dark-modern";
	case ThemeOriginalUpdated:
	default:
		return "original-updated";
	}
}

static int ThemeModeFromSettings(const QSettings& Settings)
{
	const QString SavedThemeMode = Settings.value("ThemeMode", "").toString().trimmed().toLower();
	if (SavedThemeMode == "original-classic")
		return ThemeOriginalClassic;
	if (SavedThemeMode == "dark-modern")
		return ThemeDarkModern;
	if (SavedThemeMode == "original-updated")
		return ThemeOriginalUpdated;

	if (Settings.contains("UseDarkTheme"))
		return Settings.value("UseDarkTheme", false).toBool() ? ThemeOriginalUpdated : ThemeDarkModern;

	return ThemeOriginalUpdated;
}

static bool ThemeUsesUpdatedChrome(int ThemeModeValue)
{
	return ThemeModeValue == ThemeOriginalUpdated;
}

static bool ThemeUsesClassicChrome(int ThemeModeValue)
{
	return ThemeModeValue == ThemeOriginalClassic;
}

static bool ThemeUsesDarkModernChrome(int ThemeModeValue)
{
	return ThemeModeValue == ThemeDarkModern;
}

static int TreeRowHeightForTheme(int ThemeModeValue, bool CompactRow)
{
	if (ThemeUsesClassicChrome(ThemeModeValue))
		return CompactRow ? 22 : 22;
	return CompactRow ? 36 : 46;
}

static quint64 DirectorySizeBytes(const QString& RootPath)
{
	QFileInfo RootInfo(RootPath);
	if (!RootInfo.exists())
		return 0;
	if (RootInfo.isFile())
		return static_cast<quint64>(RootInfo.size());

	quint64 TotalBytes = 0;
	QDirIterator Iterator(RootPath, QDir::Files | QDir::NoSymLinks | QDir::Hidden, QDirIterator::Subdirectories);
	while (Iterator.hasNext())
	{
		Iterator.next();
		TotalBytes += static_cast<quint64>(Iterator.fileInfo().size());
	}
	return TotalBytes;
}

static QString FormatDataSizeBytes(quint64 Bytes)
{
	const double ByteCount = static_cast<double>(Bytes);
	if (ByteCount >= 1024.0 * 1024.0 * 1024.0)
		return QString("%1 GB").arg(ByteCount / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
	if (ByteCount >= 1024.0 * 1024.0)
		return QString("%1 MB").arg(ByteCount / (1024.0 * 1024.0), 0, 'f', 1);
	if (ByteCount >= 1024.0)
		return QString("%1 KB").arg(ByteCount / 1024.0, 0, 'f', 1);
	return QString("%1 B").arg(Bytes);
}

static void AppendStartupTrace(const QString& Message)
{
	if (!qEnvironmentVariableIsSet("ML_STARTUP_TRACE"))
		return;

	QFile TraceFile(QDir::cleanPath(QDir::tempPath() + "/ModLauncher_custom_startup.log"));
	if (!TraceFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
		return;

	QTextStream Stream(&TraceFile);
	Stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << "Z " << Message << '\n';
}

static QString DefaultDisplayNameForEntryName(const QString& Name)
{
	QString DisplayName = Name.trimmed();
	DisplayName.replace('_', ' ');
	DisplayName.replace('-', ' ');
	DisplayName = DisplayName.simplified();
	if (DisplayName.isEmpty())
		DisplayName = "No Map";
	return DisplayName;
}

static QString DefaultDisplayColorForEntryName(const QString& Name, bool IsMap)
{
	const uint HashValue = qHash(Name.toLower());
	const int Hue = static_cast<int>(HashValue % 360);
	const int Saturation = IsMap ? 180 : 150;
	const int Lightness = IsMap ? 175 : 160;
	return QColor::fromHsl(Hue, Saturation, Lightness).name();
}

static bool IsPowerOfTwoDimension(int Value)
{
	return Value > 0 && (Value & (Value - 1)) == 0;
}

class QuickLaunchPicker : public QWidget
{
public:
	struct Entry
	{
		QString Category;
		QString Label;
		QString Code;
		QString Detail;
		QString SearchKey;
		QString WorkshopId;
	};

	QuickLaunchPicker(QWidget* Parent = NULL)
		: QWidget(Parent)
		, mButton(new QToolButton(this))
		, mPopup(new QFrame(NULL, Qt::Popup))
		, mSearchEdit(new QLineEdit(mPopup))
		, mTabs(new QTabBar(mPopup))
		, mListWidget(new QListWidget(mPopup))
		, mSelectedIndex(-1)
		, mBatchUpdateDepth(0)
		, mEntriesLoaded(false)
		, mWorkshopEntriesLoaded(false)
		, mButtonHeight(34)
		, mPopupHeight(340)
	{
		QHBoxLayout* Layout = new QHBoxLayout(this);
		Layout->setContentsMargins(0, 0, 0, 0);
		Layout->setSpacing(0);

		mButton->setObjectName("QuickLaunchButton");
		mButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		mButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		mButton->setCursor(Qt::PointingHandCursor);
		mButton->setText("No Map");
		Layout->addWidget(mButton);

		mPopup->setObjectName("QuickLaunchPopup");
		mPopup->setAttribute(Qt::WA_DeleteOnClose, false);
		mPopup->setMinimumWidth(560);
		mPopup->setMinimumHeight(mPopupHeight);
		QVBoxLayout* PopupLayout = new QVBoxLayout(mPopup);
		PopupLayout->setContentsMargins(10, 10, 10, 10);
		PopupLayout->setSpacing(8);

		QHBoxLayout* SearchLayout = new QHBoxLayout();
		SearchLayout->setContentsMargins(0, 0, 0, 0);
		SearchLayout->setSpacing(8);
		mSearchEdit->setObjectName("QuickLaunchSearch");
		mSearchEdit->setPlaceholderText("Search maps, IDs, or codes...");
		mSearchEdit->setClearButtonEnabled(true);
		SearchLayout->addWidget(mSearchEdit, 1);
		QToolButton* NoMapButton = new QToolButton(mPopup);
		NoMapButton->setText("No Map");
		NoMapButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		NoMapButton->setCursor(Qt::PointingHandCursor);
		NoMapButton->setAutoRaise(true);
		SearchLayout->addWidget(NoMapButton);
		PopupLayout->addLayout(SearchLayout);

		mTabs->setObjectName("QuickLaunchTabs");
		mTabs->setDocumentMode(true);
		mTabs->setDrawBase(false);
		mTabs->setExpanding(false);
		PopupLayout->addWidget(mTabs);

		mListWidget->setObjectName("QuickLaunchList");
		mListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
		mListWidget->setAlternatingRowColors(false);
		mListWidget->setUniformItemSizes(false);
		mListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		PopupLayout->addWidget(mListWidget, 1);

		connect(mButton, &QToolButton::clicked, this, [=]()
		{
			if (mPopup->isVisible())
				mPopup->hide();
			else
				ShowPopup();
		});
		connect(NoMapButton, &QToolButton::clicked, this, [=]()
		{
			for (int EntryIndex = 0; EntryIndex < mEntries.count(); ++EntryIndex)
			{
				const Entry& Item = mEntries[EntryIndex];
				if (Item.Label.compare("No Map", Qt::CaseInsensitive) == 0 && Item.Code.isEmpty())
				{
					SelectIndex(EntryIndex, true);
					mPopup->hide();
					return;
				}
			}
			SelectIndex(-1, true);
			mPopup->hide();
		});
		connect(mTabs, &QTabBar::currentChanged, this, [=](int)
		{
			if (mTabs->currentIndex() >= 0 && mTabs->tabData(mTabs->currentIndex()).toString() == "Workshop")
			{
				const bool NeedsWorkshopLoad = !mWorkshopEntriesLoaded;
				QTimer::singleShot(0, this, [this, NeedsWorkshopLoad]()
				{
					if (NeedsWorkshopLoad)
						EnsureWorkshopEntriesLoaded();
					else if (mPopup->isVisible())
						RefreshList();
				});
				return;
			}
			RefreshList();
		});
		connect(mListWidget, &QListWidget::itemClicked, this, [=](QListWidgetItem* Item)
		{
			if (!Item)
				return;
			SelectIndex(Item->data(Qt::UserRole).toInt(), true);
			mPopup->hide();
		});
		connect(mListWidget, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem* Item)
		{
			if (!Item)
				return;
			SelectIndex(Item->data(Qt::UserRole).toInt(), true);
			mPopup->hide();
		});

		SetControlHeights(mButtonHeight, mPopupHeight);
		EnsureTabs();
		UpdateButtonText();
	}

	void BeginBatchUpdate()
	{
		mBatchUpdateDepth++;
	}

	void EndBatchUpdate()
	{
		if (mBatchUpdateDepth <= 0)
			return;

		mBatchUpdateDepth--;
		if (mBatchUpdateDepth == 0)
		{
			EnsureTabs();
			UpdateButtonText();
			if (mPopup->isVisible())
				RefreshList();
		}
	}

	void AddEntry(const QString& Category, const QString& Label, const QString& Code, const QString& Detail = QString(), const QString& WorkshopId = QString())
	{
		Entry NewEntry;
		NewEntry.Category = Category.trimmed().isEmpty() ? "General" : Category.trimmed();
		NewEntry.Label = Label.trimmed().isEmpty() ? Code.trimmed() : Label.trimmed();
		NewEntry.Code = Code.trimmed();
		NewEntry.Detail = Detail.trimmed();
		NewEntry.WorkshopId = WorkshopId.trimmed();
		NewEntry.SearchKey = QString("%1 %2 %3 %4 %5").arg(NewEntry.Category, NewEntry.Label, NewEntry.Code, NewEntry.Detail, NewEntry.WorkshopId).toLower();
		mEntries.append(NewEntry);
		if (mSelectedIndex < 0)
			mSelectedIndex = 0;
		if (mBatchUpdateDepth > 0)
			return;
		EnsureTabs();
		UpdateButtonText();
		if (mPopup->isVisible())
			RefreshList();
	}

	void SetSelectionChangedHandler(const std::function<void(const QString&, const QString&)>& Handler)
	{
		mSelectionChangedHandler = Handler;
	}

	void SetPopulateEntriesHandler(const std::function<void()>& Handler)
	{
		mPopulateEntriesHandler = Handler;
	}

	void SetPopulateWorkshopEntriesHandler(const std::function<void()>& Handler)
	{
		mPopulateWorkshopEntriesHandler = Handler;
	}

	void SetControlHeights(int ButtonHeight, int PopupHeight)
	{
		mButtonHeight = qMax(30, ButtonHeight);
		mPopupHeight = qMax(300, PopupHeight);
		setMinimumHeight(mButtonHeight);
		setMaximumHeight(mButtonHeight);
		setMinimumWidth(0);
		mButton->setMinimumHeight(mButtonHeight);
		mButton->setMaximumHeight(mButtonHeight);
		mButton->setMinimumWidth(0);
		mPopup->setMinimumHeight(mPopupHeight);
		mPopup->resize(qMax(width(), 560), mPopupHeight);
	}

	QString currentCode() const
	{
		return (mSelectedIndex >= 0 && mSelectedIndex < mEntries.count()) ? mEntries[mSelectedIndex].Code : QString();
	}

	QString currentWorkshopId() const
	{
		return (mSelectedIndex >= 0 && mSelectedIndex < mEntries.count()) ? mEntries[mSelectedIndex].WorkshopId : QString();
	}

	bool IsWorkshopSelection() const
	{
		return mSelectedIndex >= 0 && mSelectedIndex < mEntries.count() && mEntries[mSelectedIndex].Category == "Workshop";
	}

	void EnsureEntriesLoaded()
	{
		if (mEntriesLoaded || !mPopulateEntriesHandler)
			return;

		mEntriesLoaded = true;
		mPopulateEntriesHandler();
	}

	void EnsureWorkshopEntriesLoaded()
	{
		if (mWorkshopEntriesLoaded || !mPopulateWorkshopEntriesHandler)
			return;

		mWorkshopEntriesLoaded = true;
		mPopulateWorkshopEntriesHandler();
	}

private:
	void EnsureTabs()
	{
		QString CurrentCategory = mTabs->currentIndex() >= 0 ? mTabs->tabData(mTabs->currentIndex()).toString() : QString("All");
		QStringList Categories = QStringList() << "Zombies" << "MP" << "Campaign/Freerun/Nightmare";
		for (const Entry& Item : mEntries)
		{
			if (Item.Category.compare("Workshop", Qt::CaseInsensitive) == 0)
			{
				Categories << "Workshop";
				break;
			}
		}
		Categories << "All";
		QSignalBlocker TabsSignalBlocker(mTabs);
		while (mTabs->count() > 0)
			mTabs->removeTab(0);
		for (const QString& Category : Categories)
		{
			const int TabIndex = mTabs->addTab(Category);
			mTabs->setTabData(TabIndex, Category);
		}
		for (int TabIndex = 0; TabIndex < mTabs->count(); ++TabIndex)
		{
			if (mTabs->tabData(TabIndex).toString() == CurrentCategory)
			{
				mTabs->setCurrentIndex(TabIndex);
				return;
			}
		}
		mTabs->setCurrentIndex(0);
	}

	void RefreshList()
	{
		mListWidget->clear();
		const QString SelectedCategory = mTabs->currentIndex() >= 0 ? mTabs->tabData(mTabs->currentIndex()).toString() : QString("All");
		const QString SearchText = mSearchEdit->text().trimmed().toLower();
		for (int EntryIndex = 0; EntryIndex < mEntries.count(); ++EntryIndex)
		{
			const Entry& Item = mEntries[EntryIndex];
			if (SelectedCategory != "All" && Item.Category != SelectedCategory)
				continue;
			if (!SearchText.isEmpty() && !Item.SearchKey.contains(SearchText))
				continue;

			QString ItemText = Item.Label;
			if (!Item.Code.isEmpty() && Item.Label.compare("No Map", Qt::CaseInsensitive) != 0)
				ItemText += QString(" (%1)").arg(Item.Code);
			QListWidgetItem* ListItem = new QListWidgetItem(ItemText, mListWidget);
			ListItem->setData(Qt::UserRole, EntryIndex);
			ListItem->setToolTip(Item.Detail.isEmpty() ? Item.Code : QString("%1\n%2").arg(Item.Code, Item.Detail));
			ListItem->setSizeHint(QSize(0, 24));
			if (EntryIndex == mSelectedIndex)
				mListWidget->setCurrentItem(ListItem);
		}
	}

	void SelectIndex(int EntryIndex, bool Notify)
	{
		if (EntryIndex < 0 || EntryIndex >= mEntries.count())
			return;
		mSelectedIndex = EntryIndex;
		UpdateButtonText();
		if (mPopup->isVisible())
			RefreshList();
		if (Notify && mSelectionChangedHandler)
			mSelectionChangedHandler(mEntries[EntryIndex].Code, mEntries[EntryIndex].WorkshopId);
	}

	void ShowPopup()
	{
		EnsureEntriesLoaded();
		const QPoint GlobalBottomLeft = mapToGlobal(QPoint(0, height()));
		QScreen* PopupScreen = screen();
		if (!PopupScreen && window() && window()->windowHandle())
			PopupScreen = window()->windowHandle()->screen();
		if (!PopupScreen)
			PopupScreen = QGuiApplication::primaryScreen();
		const QRect AvailableGeometry = PopupScreen ? PopupScreen->availableGeometry() : QRect(GlobalBottomLeft, QSize(1280, 720));
		const int PopupWidth = qMax(width() + 90, 560);
		const int PopupHeight = qMax(220, qMin(mPopupHeight, AvailableGeometry.height() - 40));
		QPoint PopupPosition = GlobalBottomLeft;
		if (PopupPosition.x() + PopupWidth > AvailableGeometry.right())
			PopupPosition.setX(qMax(AvailableGeometry.left(), AvailableGeometry.right() - PopupWidth));
		if (PopupPosition.y() + PopupHeight > AvailableGeometry.bottom())
			PopupPosition.setY(qMax(AvailableGeometry.top(), mapToGlobal(QPoint(0, 0)).y() - PopupHeight));
		mPopup->setGeometry(QRect(PopupPosition, QSize(PopupWidth, PopupHeight)));
		mPopup->show();
		mSearchEdit->setFocus();
		mSearchEdit->selectAll();
		RefreshList();
	}

	void UpdateButtonText()
	{
		const QString Label = (mSelectedIndex >= 0 && mSelectedIndex < mEntries.count()) ? mEntries[mSelectedIndex].Label : QString("No Map");
		mButton->setText(Label);
		mButton->setToolTip(Label);
	}

	QToolButton* mButton;
	QFrame* mPopup;
	QLineEdit* mSearchEdit;
	QTabBar* mTabs;
	QListWidget* mListWidget;
	QList<Entry> mEntries;
	std::function<void(const QString&, const QString&)> mSelectionChangedHandler;
	std::function<void()> mPopulateEntriesHandler;
	std::function<void()> mPopulateWorkshopEntriesHandler;
	int mSelectedIndex;
	int mBatchUpdateDepth;
	bool mEntriesLoaded;
	bool mWorkshopEntriesLoaded;
	int mButtonHeight;
	int mPopupHeight;
};

void mlMainWindow::PopulateQuickLaunchEntries()
{
	if (!mQuickLaunchWidget)
		return;

	AppendStartupTrace("quick-launch:populate-start");
	mQuickLaunchWidget->BeginBatchUpdate();
	auto AddQuickLaunchEntry = [this](const QString& Category, const QString& Label, const QString& Code, const QString& Detail = QString(), const QString& WorkshopId = QString())
	{
		mQuickLaunchWidget->AddEntry(Category, Label, Code, Detail, WorkshopId);
	};
	AddQuickLaunchEntry("General", "No Map", "");
	AddQuickLaunchEntry("Zombies", "Shadows of Evil", "zm_zod");
	AddQuickLaunchEntry("Zombies", "Der Eisendrache", "zm_castle");
	AddQuickLaunchEntry("Zombies", "Zetsubou No Shima", "zm_island");
	AddQuickLaunchEntry("Zombies", "Gorod Krovi", "zm_stalingrad");
	AddQuickLaunchEntry("Zombies", "Revelations", "zm_genesis");
	AddQuickLaunchEntry("Zombies", "Nacht der Untoten", "zm_prototype");
	AddQuickLaunchEntry("Zombies", "Verruckt", "zm_asylum");
	AddQuickLaunchEntry("Zombies", "Shi No Numa", "zm_sumpf");
	AddQuickLaunchEntry("Zombies", "Kino der Toten", "zm_theater");
	AddQuickLaunchEntry("Zombies", "Ascension", "zm_cosmodrome");
	AddQuickLaunchEntry("Zombies", "Shangri-La", "zm_temple");
	AddQuickLaunchEntry("Zombies", "Moon", "zm_moon");
	AddQuickLaunchEntry("Zombies", "Origins", "zm_tomb");
	AddQuickLaunchEntry("Zombies", "Dead Ops Arcade 2", "cp_doa_bo3");
	AddQuickLaunchEntry("MP", "Aquarium", "mp_biodome");
	AddQuickLaunchEntry("MP", "Breach", "mp_spire");
	AddQuickLaunchEntry("MP", "Combine", "mp_sector");
	AddQuickLaunchEntry("MP", "Evac", "mp_apartments");
	AddQuickLaunchEntry("MP", "Exodus", "mp_chinatown");
	AddQuickLaunchEntry("MP", "Fringe", "mp_veiled");
	AddQuickLaunchEntry("MP", "Havoc", "mp_havoc");
	AddQuickLaunchEntry("MP", "Hunted", "mp_ethiopia");
	AddQuickLaunchEntry("MP", "Infection", "mp_infection");
	AddQuickLaunchEntry("MP", "Metro", "mp_metro");
	AddQuickLaunchEntry("MP", "Redwood", "mp_redwood");
	AddQuickLaunchEntry("MP", "Stronghold", "mp_stonghold");
	AddQuickLaunchEntry("MP", "Nuk3town", "mp_nuketown_x");
	AddQuickLaunchEntry("MP", "Rise", "mp_rise");
	AddQuickLaunchEntry("MP", "Splash", "mp_waterpark");
	AddQuickLaunchEntry("MP", "Skyjacked", "mp_skyjacked");
	AddQuickLaunchEntry("MP", "Gauntlet", "mp_crucible");
	AddQuickLaunchEntry("MP", "Knockout", "mp_kung_fu");
	AddQuickLaunchEntry("MP", "Rift", "mp_conduit");
	AddQuickLaunchEntry("MP", "Spire", "mp_aerospace");
	AddQuickLaunchEntry("MP", "Verge", "mp_banzai");
	AddQuickLaunchEntry("MP", "Rumble", "mp_arena");
	AddQuickLaunchEntry("MP", "Berserk", "mp_shrine");
	AddQuickLaunchEntry("MP", "Cryogen", "mp_cryogen");
	AddQuickLaunchEntry("MP", "Empire", "mp_rome");
	AddQuickLaunchEntry("MP", "Micro", "mp_miniature");
	AddQuickLaunchEntry("MP", "Rupture", "mp_city");
	AddQuickLaunchEntry("MP", "Outlaw", "mp_western");
	AddQuickLaunchEntry("MP", "Citadel", "mp_ruins");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Safehouse: Mobile", "cp_sh_mobile");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Safehouse: Singapore", "cp_sh_singapore");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Safehouse: Cairo", "cp_sh_cairo");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Black Ops", "cp_mi_eth_prologue");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "New World", "cp_mi_zurich_newworld");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "In Darkness", "cp_mi_sing_blackstation");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Provocation", "cp_mi_sing_biodomes");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Hypocenter", "cp_mi_sing_sgen");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Vengeance", "cp_mi_sing_vengeance");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Rise & Fall", "cp_mi_cairo_ramses");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Demon Within", "cp_mi_cairo_infection");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Sand Castle", "cp_mi_cairo_aquifer");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Lotus Towers", "cp_mi_cairo_lotus");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Life", "cp_mi_zurich_coalescence");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Freerun: Alpha", "mp_freerun_01");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Freerun: Sidewinder", "mp_freerun_02");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Freerun: Infected", "mp_freerun_03");
	AddQuickLaunchEntry("Campaign/Freerun/Nightmare", "Freerun: Blackout", "mp_freerun_04");

	mQuickLaunchWidget->EndBatchUpdate();
	UpdateQuickLaunchVisibility();
	AppendStartupTrace("quick-launch:populate-done");
}

void mlMainWindow::PopulateWorkshopQuickLaunchEntries()
{
	if (!mQuickLaunchWidget)
		return;

	AppendStartupTrace("quick-launch:workshop-populate-start");
	mQuickLaunchWidget->BeginBatchUpdate();

	QSet<QString> ExistingQuickLaunchCodes;
	ExistingQuickLaunchCodes.insert(QString());
	for (const QString& BaseMapCode : QStringList()
		<< "zm_zod" << "zm_factory" << "zm_castle" << "zm_island" << "zm_stalingrad" << "zm_genesis" << "zm_prototype" << "zm_asylum"
		<< "zm_sumpf" << "zm_theater" << "zm_cosmodrome" << "zm_temple" << "zm_moon" << "zm_tomb" << "cp_doa_bo3"
		<< "mp_biodome" << "mp_spire" << "mp_sector" << "mp_apartments" << "mp_chinatown" << "mp_veiled" << "mp_havoc" << "mp_ethiopia"
		<< "mp_infection" << "mp_metro" << "mp_redwood" << "mp_stonghold" << "mp_nuketown_x" << "mp_rise" << "mp_waterpark"
		<< "mp_skyjacked" << "mp_crucible" << "mp_kung_fu" << "mp_conduit" << "mp_aerospace" << "mp_banzai" << "mp_arena"
		<< "mp_shrine" << "mp_cryogen" << "mp_rome" << "mp_miniature" << "mp_city" << "mp_western" << "mp_ruins"
		<< "cp_sh_mobile" << "cp_sh_singapore" << "cp_sh_cairo" << "cp_mi_eth_prologue" << "cp_mi_zurich_newworld"
		<< "cp_mi_sing_blackstation" << "cp_mi_sing_biodomes" << "cp_mi_sing_sgen" << "cp_mi_sing_vengeance" << "cp_mi_cairo_ramses"
		<< "cp_mi_cairo_infection" << "cp_mi_cairo_aquifer" << "cp_mi_cairo_lotus" << "cp_mi_zurich_coalescence"
		<< "mp_freerun_01" << "mp_freerun_02" << "mp_freerun_03" << "mp_freerun_04")
	{
		ExistingQuickLaunchCodes.insert(BaseMapCode);
	}

	QDir WorkshopRoot(mGamePath);
	if (WorkshopRoot.cdUp() && WorkshopRoot.cdUp())
	{
		const QString WorkshopMapsPath = QDir::cleanPath(QString("%1/workshop/content/%2").arg(WorkshopRoot.absolutePath()).arg(AppId));
		QDir WorkshopMapsDir(WorkshopMapsPath);
		if (WorkshopMapsDir.exists())
		{
			AppendStartupTrace(QString("quick-launch:workshop-scan-start path=%1").arg(WorkshopMapsPath));
			QList<QPair<QString, QString>> WorkshopEntries;
			const QStringList WorkshopIds = WorkshopMapsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
			for (const QString& WorkshopId : WorkshopIds)
			{
				AppendStartupTrace(QString("quick-launch:workshop-item-start id=%1").arg(WorkshopId));
				const QString WorkshopItemPath = QDir::cleanPath(QString("%1/%2").arg(WorkshopMapsPath, WorkshopId));
				QString MapCode;
				QString MapTitle;
				QFile WorkshopJsonFile(QDir::cleanPath(QString("%1/workshop.json").arg(WorkshopItemPath)));
				if (WorkshopJsonFile.open(QIODevice::ReadOnly))
				{
					const QJsonObject WorkshopJson = QJsonDocument::fromJson(WorkshopJsonFile.readAll()).object();
					const QString WorkshopType = WorkshopJson.value("Type").toString().trimmed().toLower();
					if (!WorkshopType.isEmpty() && WorkshopType != "map")
						continue;
					MapCode = WorkshopJson.value("FolderName").toString().trimmed();
					MapTitle = WorkshopJson.value("Title").toString().trimmed();
				}
				if (MapCode.isEmpty())
				{
					const QStringList FastFiles = QDir(WorkshopItemPath).entryList(QStringList() << "*.ff", QDir::Files, QDir::Name);
					if (!FastFiles.isEmpty())
						MapCode = QFileInfo(FastFiles.first()).completeBaseName();
				}
				const QString NormalizedMapCode = MapCode.trimmed().toLower();
				if (NormalizedMapCode.isEmpty() || ExistingQuickLaunchCodes.contains(NormalizedMapCode))
					continue;
				ExistingQuickLaunchCodes.insert(NormalizedMapCode);
				if (MapTitle.isEmpty())
					MapTitle = DefaultDisplayNameForEntryName(MapCode);
				WorkshopEntries.append(qMakePair(MapTitle, QString("%1|%2").arg(MapCode, WorkshopId)));
			}
			std::sort(WorkshopEntries.begin(), WorkshopEntries.end(), [](const QPair<QString, QString>& Left, const QPair<QString, QString>& Right)
			{
				return Left.first.toLower() < Right.first.toLower();
			});
			for (const QPair<QString, QString>& WorkshopEntry : WorkshopEntries)
				mQuickLaunchWidget->AddEntry("Workshop", WorkshopEntry.first, WorkshopEntry.second.section('|', 0, 0), QString("Workshop ID: %1").arg(WorkshopEntry.second.section('|', 1, 1)), WorkshopEntry.second.section('|', 1, 1));
			AppendStartupTrace(QString("quick-launch:workshop-scan-done count=%1").arg(WorkshopEntries.count()));
		}
	}

	mQuickLaunchWidget->EndBatchUpdate();
	UpdateQuickLaunchVisibility();
	AppendStartupTrace("quick-launch:workshop-populate-done");
}

static const char* kThemeProfileSettingKey = "SelectedThemeProfile";
static const char* kThemeProfilesGroup = "ThemeProfiles";

static QString BuiltInThemeProfileId(int ThemeModeValue)
{
	switch (ThemeModeValue)
	{
	case ThemeOriginalClassic:
		return "original-classic";
	case ThemeDarkModern:
		return "dark-modern";
	case ThemeOriginalUpdated:
	default:
		return "original-classic";
	}
}

static int ThemeModeFromProfileId(const QString& ThemeProfileId)
{
	if (ThemeProfileId == "original-classic")
		return ThemeOriginalClassic;
	if (ThemeProfileId == "dark-modern")
		return ThemeDarkModern;
	return ThemeOriginalUpdated;
}

static QString ThemeProfileDisplayNameForBuiltInId(const QString& ThemeProfileId)
{
	if (ThemeProfileId == "original-classic")
		return "Classic Upgrade";
	if (ThemeProfileId == "dark-modern")
		return "Dark Modern";
	return "Original Updated";
}

static QStringList ThemeProfileSettingKeys()
{
	return QStringList()
		<< "ThemeMode"
		<< "AccentColor"
		<< "ShowItemTypeTags"
		<< "CustomStylesheet"
		<< "ConsoleStyle"
		<< "AssetTreeBackgroundImage"
		<< "AssetTreeBackgroundOpacity"
		<< "LogBackgroundImage"
		<< "LogBackgroundOpacity"
		<< "UseLegacyToolbarIcons"
		<< "LauncherLayout"
		<< "LogColors/Default"
		<< "LogColors/Command"
		<< "LogColors/Info"
		<< "LogColors/Launch"
		<< "LogColors/Success"
		<< "LogColors/Warning"
		<< "LogColors/Error";
}

static QVariantMap DefaultThemeProfileValues(const QString& ThemeProfileId)
{
	QVariantMap Values;
	Values.insert("ThemeMode", ThemeModeToSettingsValue(ThemeModeFromProfileId(ThemeProfileId)));
	Values.insert("AccentColor", "#ff8a2a");
	Values.insert("ShowItemTypeTags", true);
	Values.insert("CustomStylesheet", "");
	Values.insert("ConsoleStyle", "improved");
	Values.insert("AssetTreeBackgroundImage", "");
	Values.insert("AssetTreeBackgroundOpacity", 100);
	Values.insert("LogBackgroundImage", "");
	Values.insert("LogBackgroundOpacity", 100);
	Values.insert("UseLegacyToolbarIcons", false);
	Values.insert("LauncherLayout", "modern");
	Values.insert("LogColors/Default", "#d7dce2");
	Values.insert("LogColors/Command", "#7dcfff");
	Values.insert("LogColors/Info", "#eef1f4");
	Values.insert("LogColors/Launch", "#c792ea");
	Values.insert("LogColors/Success", "#6ee7a8");
	Values.insert("LogColors/Warning", "#ffcf70");
	Values.insert("LogColors/Error", "#ff7a7a");
	return Values;
}

static QString SanitizedThemeProfileId(const QString& DisplayName)
{
	QString ThemeId = DisplayName.trimmed().toLower();
	ThemeId.replace(QRegularExpression("[^a-z0-9]+"), "-");
	while (ThemeId.contains("--"))
		ThemeId.replace("--", "-");
	ThemeId.remove(QRegularExpression("^-+|-+$"));
	if (ThemeId.isEmpty())
		ThemeId = "custom-theme";
	return ThemeId;
}

enum mlItemType
{
	ML_ITEM_UNKNOWN,
	ML_ITEM_MAP,
	ML_ITEM_MOD,
	ML_ITEM_MOD_GROUP
};

enum mlItemDataRole
{
	ML_ITEM_CONTAINER_ROLE = Qt::UserRole + 1,
	ML_ITEM_NAME_ROLE,
	ML_ITEM_FAVORITE_ROLE,
	ML_ITEM_CHECKSTATE_ROLE
};

enum mlLogDataRole
{
	ML_LOG_TEXT_ROLE = Qt::UserRole + 100,
	ML_LOG_IS_HEADER_ROLE,
	ML_LOG_EXPANDED_ROLE,
	ML_LOG_TEXT_COLOR_ROLE,
	ML_LOG_BG_COLOR_ROLE
};

static QString StripTreyarchColorCodes(const QString& Title)
{
	QString CleanTitle = Title;
	CleanTitle.remove(QRegularExpression("\\^[0-9]"));
	return CleanTitle.trimmed();
}

static QString SteamDescriptionForUpload(const QString& BriefingDescription, const QString& SteamDescription)
{
	return SteamDescription.trimmed().isEmpty() ? BriefingDescription : SteamDescription;
}

static bool IsGameLaunchCommand(const QString& ProgramPath, const QStringList& Args)
{
	const QString FileName = QFileInfo(ProgramPath).fileName();
	return FileName.compare("BlackOps3.exe", Qt::CaseInsensitive) == 0
		|| (FileName.compare("Steam.exe", Qt::CaseInsensitive) == 0 && Args.contains("-applaunch") && Args.contains(QString::number(AppId)));
}

struct GameWindowSearchState
{
	DWORD ProcessId;
	bool HasVisibleWindow;
};

class BackgroundDropLineEdit : public QLineEdit
{
public:
	BackgroundDropLineEdit(const QString& Text = QString(), QWidget* Parent = NULL)
		: QLineEdit(Text, Parent)
	{
		setAcceptDrops(true);
	}

protected:
	void dragEnterEvent(QDragEnterEvent* Event)
	{
		if (Event->mimeData()->hasUrls())
			Event->acceptProposedAction();
		else
			QLineEdit::dragEnterEvent(Event);
	}

	void dropEvent(QDropEvent* Event)
	{
		if (Event->mimeData()->hasUrls() && Event->mimeData()->urls().count())
		{
			const QString LocalPath = Event->mimeData()->urls().first().toLocalFile();
			if (!LocalPath.isEmpty())
			{
				setText(QDir::toNativeSeparators(LocalPath));
				Event->acceptProposedAction();
				return;
			}
		}

		QLineEdit::dropEvent(Event);
	}
};

class HoverRevealWidget : public QWidget
{
public:
	HoverRevealWidget(QWidget* Parent = NULL)
		: QWidget(Parent), mRevealWidget(NULL)
	{
		setAttribute(Qt::WA_Hover, true);
	}

	void SetRevealWidget(QWidget* Widget)
	{
		mRevealWidget = Widget;
		if (mRevealWidget)
			mRevealWidget->setVisible(false);
	}

protected:
	void resizeEvent(QResizeEvent* Event)
	{
		QWidget::resizeEvent(Event);
	}

	void enterEvent(QEnterEvent* Event)
	{
		setProperty("hovered", true);
		style()->unpolish(this);
		style()->polish(this);
		update();
		if (mRevealWidget)
			mRevealWidget->setVisible(true);
		QWidget::enterEvent(Event);
	}

	void leaveEvent(QEvent* Event)
	{
		setProperty("hovered", false);
		style()->unpolish(this);
		style()->polish(this);
		update();
		if (mRevealWidget)
			mRevealWidget->setVisible(false);
		QWidget::leaveEvent(Event);
	}

private:
	QWidget* mRevealWidget;
};

class CategoryTabBar : public QTabBar
{
public:
	CategoryTabBar(QWidget* Parent = NULL)
		: QTabBar(Parent)
	{
	}

protected:
	void paintEvent(QPaintEvent* Event) override
	{
		Q_UNUSED(Event);

		QStylePainter Painter(this);
		QStyleOptionTab Option;
		for (int TabIdx = 0; TabIdx < count(); ++TabIdx)
		{
			initStyleOption(&Option, TabIdx);
			const QColor TabColor = tabTextColor(TabIdx);
			if (TabColor.isValid())
			{
				Option.palette.setColor(QPalette::WindowText, TabColor);
				Option.palette.setColor(QPalette::ButtonText, TabColor);
				Option.palette.setColor(QPalette::Text, TabColor);
			}

			Painter.drawControl(QStyle::CE_TabBarTabShape, Option);
			Painter.drawControl(QStyle::CE_TabBarTabLabel, Option);

			// Force the final visible label color to match setTabTextColor even when global stylesheets set a tab text color.
			QRect TextRect = style()->subElementRect(QStyle::SE_TabBarTabText, &Option, this);
			QString DrawText = fontMetrics().elidedText(Option.text, elideMode(), qMax(0, TextRect.width()));
			Painter.save();
			Painter.setFont(font());
			Painter.setPen(TabColor.isValid() ? TabColor : palette().color(QPalette::ButtonText));
			Painter.drawText(TextRect, Qt::AlignCenter | Qt::TextShowMnemonic, DrawText);
			Painter.restore();
		}
	}
};

static void UpdateBackgroundPreviewLabel(QLabel* PreviewLabel, const QString& FileName)
{
	if (!PreviewLabel)
		return;

	PreviewLabel->setText("None");
	PreviewLabel->setPixmap(QPixmap());

	const QString NormalizedPath = FileName.trimmed();
	if (NormalizedPath.isEmpty())
		return;

	QString PreviewPath = NormalizedPath;
	if (NormalizedPath.startsWith("qrc:/"))
		PreviewPath = ":" + NormalizedPath.mid(4);

	const QPixmap SourcePixmap(PreviewPath);
	if (SourcePixmap.isNull())
	{
		PreviewLabel->setText("Invalid");
		return;
	}

	PreviewLabel->setPixmap(SourcePixmap.scaled(PreviewLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
}

static QString SanitizedFileName(QString Value)
{
	Value = Value.trimmed();
	Value.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "_");
	while (Value.contains("__"))
		Value.replace("__", "_");
	Value = Value.trimmed();
	if (Value.isEmpty())
		Value = "default";
	return Value;
}

static QString HumanizedVersionLabel(const QString& Value)
{
	QString Label = Value.trimmed();
	if (Label.isEmpty())
		return Label;
	Label.replace('_', ' ');
	Label.replace('-', ' ');
	Label.replace(QRegularExpression("\\s+"), " ");
	return Label.trimmed();
}

static bool EditJsonTextDialog(QWidget* Parent, const QString& FilePath, const QString& Title)
{
	QFile File(FilePath);
	if (!File.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(Parent, Title, QString("Unable to open '%1'.").arg(FilePath));
		return false;
	}

	QDialog Dialog(Parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle(Title);
	Dialog.resize(880, 680);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QLabel* PathLabel = new QLabel(QDir::toNativeSeparators(FilePath), &Dialog);
	PathLabel->setWordWrap(true);
	Layout->addWidget(PathLabel);

	QTextEdit* Editor = new QTextEdit(&Dialog);
	Editor->setAcceptRichText(false);
	Editor->setPlainText(QString::fromUtf8(File.readAll()));
	Layout->addWidget(Editor, 1);

	QDialogButtonBox* Buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &Dialog);
	Layout->addWidget(Buttons);
	QObject::connect(Buttons, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	QObject::connect(Buttons, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return false;

	QJsonParseError ParseError;
	const QByteArray UpdatedBytes = Editor->toPlainText().toUtf8();
	QJsonDocument Parsed = QJsonDocument::fromJson(UpdatedBytes, &ParseError);
	if (ParseError.error != QJsonParseError::NoError)
	{
		QMessageBox::warning(Parent, Title, QString("Invalid JSON: %1").arg(ParseError.errorString()));
		return false;
	}

	QFile OutFile(FilePath);
	if (!OutFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		QMessageBox::warning(Parent, Title, QString("Unable to write '%1'.").arg(FilePath));
		return false;
	}

	OutFile.write(Parsed.toJson(QJsonDocument::Indented));
	return true;
}

static QString SteamMarkupToHtml(QString Text)
{
	const QRegularExpression::PatternOptions MarkupOptions =
		QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption;
	Text = Text.toHtmlEscaped();
	Text.replace(QRegularExpression("\\[b\\](.*?)\\[/b\\]", MarkupOptions), "<b>\\1</b>");
	Text.replace(QRegularExpression("\\[i\\](.*?)\\[/i\\]", MarkupOptions), "<i>\\1</i>");
	Text.replace(QRegularExpression("\\[u\\](.*?)\\[/u\\]", MarkupOptions), "<u>\\1</u>");
	Text.replace(QRegularExpression("\\[strike\\](.*?)\\[/strike\\]", MarkupOptions), "<span style=\"text-decoration:line-through;\">\\1</span>");
	Text.replace(QRegularExpression("\\[url=(.*?)\\](.*?)\\[/url\\]", MarkupOptions), "<a href=\"\\1\">\\2</a>");
	Text.replace(QRegularExpression("\\[url\\](.*?)\\[/url\\]", MarkupOptions), "<a href=\"\\1\">\\1</a>");
	Text.replace(QRegularExpression("\\[img\\](.*?)\\[/img\\]", MarkupOptions), "<div style=\"margin:10px 0;\"><a href=\"\\1\">Open image</a><br/><span style=\"color:#8d96a0; font-size:11px;\">\\1</span></div>");
	Text.replace(QRegularExpression("\\[h1\\](.*?)\\[/h1\\]", MarkupOptions), "<h3>\\1</h3>");
	Text.replace(QRegularExpression("\\[quote\\](.*?)\\[/quote\\]", MarkupOptions), "<blockquote>\\1</blockquote>");
	Text.replace("[*]", "<li>");
	Text.replace(QRegularExpression("\\[list\\](.*?)\\[/list\\]", MarkupOptions), "<ul>\\1</ul>");
	Text.replace("\n", "<br/>");
	return QString("<html><body style=\"margin:8px; line-height:1.45;\">%1</body></html>").arg(Text);
}

static QString NormalizedStoredColor(QString Value)
{
	Value = Value.trimmed();
	if (Value.isEmpty())
		return QString();

	const QColor Parsed(Value);
	return Parsed.isValid() ? Parsed.name(QColor::HexRgb) : QString();
}

static bool ConfirmDestructiveActionTwice(QWidget* Parent, const QString& Title, const QString& TargetLabel, const QString& DetailText)
{
	if (QMessageBox::warning(Parent, Title, QString("You are about to delete %1.\n\n%2").arg(TargetLabel, DetailText), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return false;

	return QMessageBox::warning(Parent, Title, QString("Are you sure you want to delete %1?").arg(TargetLabel), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

static bool IsTextFileForRenameTransform(const QString& FilePath)
{
	const QString LowerName = QFileInfo(FilePath).fileName().toLower();
	static const char* Suffixes[] =
	{
		".gsc", ".csc", ".gsh", ".csv", ".csv.partial", ".zone", ".zone.partial", ".szc",
		".json", ".cfg", ".txt", ".menu", ".lua", ".map", ".bak", ".lin", ".deps",
		".errorlog", ".log", ".str", ".vision", ".atr", ".inc", ".py", ".sh", ".bat",
		".cpp", ".c", ".h", ".hpp", ".vcxproj", ".filters", ".sln", ".md"
	};
	for (int SuffixIdx = 0; SuffixIdx < static_cast<int>(sizeof(Suffixes) / sizeof(Suffixes[0])); SuffixIdx++)
	{
		if (LowerName.endsWith(Suffixes[SuffixIdx]))
			return true;
	}

	return false;
}

static bool EnsureParentFolderExists(const QString& FilePath)
{
	return QDir().mkpath(QFileInfo(FilePath).absolutePath());
}

static bool CopyFilePlain(const QString& SourcePath, const QString& TargetPath, QString* Error = NULL)
{
	if (!EnsureParentFolderExists(TargetPath))
	{
		if (Error)
			*Error = QString("Unable to create '%1'.").arg(QFileInfo(TargetPath).absolutePath());
		return false;
	}

	QFile::remove(TargetPath);
	if (!QFile::copy(SourcePath, TargetPath))
	{
		if (Error)
			*Error = QString("Unable to copy '%1' to '%2'.").arg(QDir::toNativeSeparators(SourcePath), QDir::toNativeSeparators(TargetPath));
		return false;
	}

	return true;
}

static bool CopyFileWithRenameTransform(const QString& SourcePath, const QString& TargetPath, const QString& OldName, const QString& NewName, QString* Error = NULL)
{
	if (IsTextFileForRenameTransform(SourcePath))
	{
		QFile SourceFile(SourcePath);
		if (!SourceFile.open(QIODevice::ReadOnly))
		{
			if (Error)
				*Error = QString("Unable to read '%1'.").arg(QDir::toNativeSeparators(SourcePath));
			return false;
		}

		if (!EnsureParentFolderExists(TargetPath))
		{
			if (Error)
				*Error = QString("Unable to create '%1'.").arg(QFileInfo(TargetPath).absolutePath());
			return false;
		}

		QString Text = QString::fromUtf8(SourceFile.readAll());
		Text.replace(OldName, NewName);

		QFile TargetFile(TargetPath);
		if (!TargetFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
		{
			if (Error)
				*Error = QString("Unable to write '%1'.").arg(QDir::toNativeSeparators(TargetPath));
			return false;
		}

		TargetFile.write(Text.toUtf8());
		return true;
	}

	return CopyFilePlain(SourcePath, TargetPath, Error);
}

static bool CopyDirectoryRecursive(const QString& SourceRoot, const QString& TargetRoot, QString* Error = NULL)
{
	QDir SourceDir(SourceRoot);
	if (!SourceDir.exists())
		return true;

	if (!QDir().mkpath(TargetRoot))
	{
		if (Error)
			*Error = QString("Unable to create '%1'.").arg(QDir::toNativeSeparators(TargetRoot));
		return false;
	}

	QDirIterator It(SourceRoot, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
	while (It.hasNext())
	{
		It.next();
		const QString RelativePath = SourceDir.relativeFilePath(It.filePath());
		const QString TargetPath = QDir::cleanPath(QString("%1/%2").arg(TargetRoot, RelativePath));

		if (It.fileInfo().isDir())
		{
			if (!QDir().mkpath(TargetPath))
			{
				if (Error)
					*Error = QString("Unable to create '%1'.").arg(QDir::toNativeSeparators(TargetPath));
				return false;
			}
			continue;
		}

		if (!CopyFilePlain(It.filePath(), TargetPath, Error))
			return false;
	}

	return true;
}

static bool CopyDirectoryWithRenameTransform(const QString& SourceRoot, const QString& TargetRoot, const QString& OldName, const QString& NewName, QString* Error = NULL)
{
	QDir SourceDir(SourceRoot);
	if (!SourceDir.exists())
		return true;

	if (!QDir().mkpath(TargetRoot))
	{
		if (Error)
			*Error = QString("Unable to create '%1'.").arg(QDir::toNativeSeparators(TargetRoot));
		return false;
	}

	QDirIterator It(SourceRoot, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
	while (It.hasNext())
	{
		It.next();
		QString RelativePath = SourceDir.relativeFilePath(It.filePath());
		RelativePath.replace(OldName, NewName);
		const QString TargetPath = QDir::cleanPath(QString("%1/%2").arg(TargetRoot, RelativePath));

		if (It.fileInfo().isDir())
		{
			if (!QDir().mkpath(TargetPath))
			{
				if (Error)
					*Error = QString("Unable to create '%1'.").arg(QDir::toNativeSeparators(TargetPath));
				return false;
			}
			continue;
		}

		if (!CopyFileWithRenameTransform(It.filePath(), TargetPath, OldName, NewName, Error))
			return false;
	}

	return true;
}

static QString NormalizeScriptAssetPath(QString Value)
{
	Value = Value.trimmed();
	Value.replace('\\', '/');
	while (Value.contains("//"))
		Value.replace("//", "/");
	return Value.toLower();
}

static QStringList CollectFilesWhoseRelativePathContains(const QString& RootPath, const QString& Token)
{
	QStringList Matches;
	QDir RootDir(RootPath);
	if (!RootDir.exists())
		return Matches;

	QDirIterator It(RootPath, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
	while (It.hasNext())
	{
		const QString FilePath = It.next();
		const QString RelativePath = RootDir.relativeFilePath(FilePath);
		if (RelativePath.contains(Token, Qt::CaseSensitive))
			Matches << FilePath;
	}

	return Matches;
}

static QStringList ExtractClientfieldRegistrations(const QString& Text)
{
	QStringList Fields;
	QRegularExpression Expression("clientfield::register\\s*\\(\\s*\"[^\"]+\"\\s*,\\s*(?:\"([^\"]+)\"|([A-Za-z0-9_\\.]+))", QRegularExpression::CaseInsensitiveOption);
	QRegularExpressionMatchIterator It = Expression.globalMatch(Text);
	while (It.hasNext())
	{
		const QRegularExpressionMatch Match = It.next();
		const QString Name = Match.captured(1).isEmpty() ? Match.captured(2).trimmed() : Match.captured(1).trimmed();
		if (!Name.isEmpty())
			Fields << Name.toLower();
	}
	return Fields;
}

static QString StripScriptComments(QString Text)
{
	Text.remove(QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption));
	Text.remove(QRegularExpression("(^|\\s)//[^\\n\\r]*", QRegularExpression::MultilineOption));
	Text.remove(QRegularExpression("(^|\\s)--[^\\n\\r]*", QRegularExpression::MultilineOption));
	return Text;
}

static QStringList ExtractDeclaredNamespaces(const QString& Text)
{
	QStringList Namespaces;
	QRegularExpression Expression("^\\s*#namespace\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*;", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
	QRegularExpressionMatchIterator It = Expression.globalMatch(StripScriptComments(Text));
	while (It.hasNext())
	{
		const QString NamespaceName = It.next().captured(1).trimmed().toLower();
		if (!NamespaceName.isEmpty())
			Namespaces << NamespaceName;
	}
	return Namespaces;
}

static QStringList ExtractScriptUsings(const QString& Text)
{
	QStringList Paths;
	QRegularExpression Expression("^\\s*#using\\s+([^\\s;]+)", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
	QRegularExpressionMatchIterator It = Expression.globalMatch(Text);
	while (It.hasNext())
	{
		QString Path = It.next().captured(1).trimmed();
		Path.remove(';');
		Path.remove('"');
		if (!Path.isEmpty())
			Paths << NormalizeScriptAssetPath(Path);
	}
	return Paths;
}

static QStringList ExtractNamespaceUsages(const QString& Text)
{
	QStringList Namespaces;
	QRegularExpression Expression("\\b([A-Za-z_][A-Za-z0-9_]*)::[A-Za-z_][A-Za-z0-9_]*");
	QRegularExpressionMatchIterator It = Expression.globalMatch(StripScriptComments(Text));
	while (It.hasNext())
	{
		const QString NamespaceName = It.next().captured(1).trimmed();
		if (!NamespaceName.isEmpty())
			Namespaces << NamespaceName.toLower();
	}
	return Namespaces;
}

static QStringList ExtractZoneScriptEntries(const QString& Text)
{
	QStringList Entries;
	QRegularExpression Expression("^\\s*scriptparsetree\\s*,\\s*([^\\s,]+)", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
	QRegularExpressionMatchIterator It = Expression.globalMatch(Text);
	while (It.hasNext())
	{
		const QString Entry = NormalizeScriptAssetPath(It.next().captured(1));
		if (!Entry.isEmpty())
			Entries << Entry;
	}
	return Entries;
}

static QStringList ExtractCommentedZoneEntriesForType(const QString& Text, const QString& EntryType)
{
	QStringList Entries;
	QRegularExpression Expression(
		QString("^\\s*//+\\s*%1\\s*,\\s*([^\\s,]+)").arg(QRegularExpression::escape(EntryType)),
		QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
	QRegularExpressionMatchIterator It = Expression.globalMatch(Text);
	while (It.hasNext())
	{
		const QString Entry = NormalizeScriptAssetPath(It.next().captured(1));
		if (!Entry.isEmpty())
			Entries << Entry;
	}
	return Entries;
}

static QStringList ExtractZoneEntriesForType(const QString& Text, const QString& EntryType)
{
	QStringList Entries;
	QRegularExpression Expression(
		QString("^\\s*%1\\s*,\\s*([^\\s,]+)").arg(QRegularExpression::escape(EntryType)),
		QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
	QRegularExpressionMatchIterator It = Expression.globalMatch(Text);
	while (It.hasNext())
	{
		const QString Entry = NormalizeScriptAssetPath(It.next().captured(1));
		if (!Entry.isEmpty())
			Entries << Entry;
	}
	return Entries;
}

static QString NormalizeDefaultManifestPath(QString Value)
{
	Value = NormalizeScriptAssetPath(Value);
	if (Value.startsWith("./"))
		Value.remove(0, 2);
	return Value;
}

static bool ManifestContainsUiAsset(const QSet<QString>& Manifest, const QString& AssetPath)
{
	const QString NormalizedPath = NormalizeDefaultManifestPath(QString("share/raw/%1").arg(AssetPath));
	if (Manifest.contains(NormalizedPath))
		return true;

	const int ExtensionIndex = NormalizedPath.lastIndexOf('.');
	if (ExtensionIndex < 0)
		return false;

	const QString BasePath = NormalizedPath.left(ExtensionIndex);
	const QString Extension = NormalizedPath.mid(ExtensionIndex).toLower();
	if (Extension == ".lua")
		return Manifest.contains(BasePath + ".luac") || Manifest.contains(BasePath + ".menu");
	if (Extension == ".luac")
		return Manifest.contains(BasePath + ".lua");
	return false;
}

static bool RawfilePathMatchesVariant(const QString& LeftPath, const QString& RightPath)
{
	const QString Left = NormalizeScriptAssetPath(LeftPath);
	const QString Right = NormalizeScriptAssetPath(RightPath);
	if (Left == Right)
		return true;

	const int LeftDot = Left.lastIndexOf('.');
	const int RightDot = Right.lastIndexOf('.');
	if (LeftDot < 0 || RightDot < 0)
		return false;

	const QString LeftBase = Left.left(LeftDot);
	const QString RightBase = Right.left(RightDot);
	if (LeftBase != RightBase)
		return false;

	const QString LeftExt = Left.mid(LeftDot).toLower();
	const QString RightExt = Right.mid(RightDot).toLower();
	const QStringList LuaExts = QStringList() << ".lua" << ".luac" << ".menu";
	return LuaExts.contains(LeftExt) && LuaExts.contains(RightExt);
}

static bool ZoneContainsRawfilePath(const QSet<QString>& ZoneEntries, const QString& RawfilePath)
{
	if (ZoneEntries.contains(RawfilePath))
		return true;

	for (const QString& Entry : ZoneEntries)
	{
		if (RawfilePathMatchesVariant(Entry, RawfilePath))
			return true;
	}
	return false;
}

static bool HashContainsPathVariant(const QHash<QString, QString>& Entries, const QString& AssetPath)
{
	if (Entries.contains(AssetPath))
		return true;

	for (auto It = Entries.constBegin(); It != Entries.constEnd(); ++It)
	{
		if (RawfilePathMatchesVariant(It.key(), AssetPath))
			return true;
	}
	return false;
}

static QString CanonicalDefaultUiManifestEntry(const QString& RelativePath)
{
	const QString Normalized = NormalizeDefaultManifestPath(RelativePath);
	if (Normalized.startsWith("share/raw/scripts/ui/"))
		return QString("share/raw/ui/%1").arg(Normalized.mid(QString("share/raw/scripts/ui/").length()));
	if (Normalized.startsWith("share/raw/scripts/lua/"))
		return QString("share/raw/lua/%1").arg(Normalized.mid(QString("share/raw/scripts/lua/").length()));
	return Normalized;
}

static bool IsNamespaceManifestEntry(const QString& Entry)
{
	return Entry.startsWith("namespace:");
}

static QMultiHash<QString, QString> LoadDefaultScriptNamespacesFromManifest(const QString& ManifestPath)
{
	QMultiHash<QString, QString> Namespaces;
	QFile ManifestFile(ManifestPath);
	if (!ManifestFile.open(QIODevice::ReadOnly))
		return Namespaces;

	while (!ManifestFile.atEnd())
	{
		const QString Entry = QString::fromUtf8(ManifestFile.readLine()).trimmed();
		if (!IsNamespaceManifestEntry(Entry))
			continue;

		const int EqualsIndex = Entry.indexOf('=');
		if (EqualsIndex <= 10)
			continue;

		const QString NamespaceName = Entry.mid(QString("namespace:").length(), EqualsIndex - QString("namespace:").length()).trimmed().toLower();
		const QString ScriptPath = NormalizeDefaultManifestPath(Entry.mid(EqualsIndex + 1).trimmed());
		if (!NamespaceName.isEmpty() && !ScriptPath.isEmpty())
			Namespaces.insert(NamespaceName, ScriptPath);
	}

	return Namespaces;
}

static QString LuaModuleToRawfilePath(QString ModuleName)
{
	ModuleName = ModuleName.trimmed();
	ModuleName.remove('"');
	ModuleName.remove('\'');
	if (ModuleName.isEmpty())
		return QString();

	QString Path = ModuleName;
	Path.replace('.', '/');
	if (!Path.startsWith("ui/", Qt::CaseInsensitive) && !Path.startsWith("lua/", Qt::CaseInsensitive))
		return QString();
	return NormalizeScriptAssetPath(Path + ".lua");
}

static QStringList ExtractUiRequireModules(const QString& Text)
{
	QStringList Modules;
	QRegularExpression RequireExpression("require\\s*\\(\\s*[\"']([^\"']+)[\"']\\s*\\)", QRegularExpression::CaseInsensitiveOption);
	QRegularExpression LuiLoadExpression("LuiLoad\\s*\\(\\s*[\"']([^\"']+)[\"']\\s*\\)", QRegularExpression::CaseInsensitiveOption);

	auto AppendMatches = [&](const QRegularExpression& Expression)
	{
		QRegularExpressionMatchIterator It = Expression.globalMatch(Text);
		while (It.hasNext())
		{
			const QString ModuleName = It.next().captured(1).trimmed();
			if (!ModuleName.isEmpty())
				Modules << ModuleName;
		}
	};

	AppendMatches(RequireExpression);
	AppendMatches(LuiLoadExpression);
	return Modules;
}

static QStringList ExtractWeaponTableLoads(const QString& Text)
{
	QStringList Tables;
	QRegularExpression Expression("load_weapon_spec_from_table\\s*\\(\\s*[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption);
	QRegularExpressionMatchIterator It = Expression.globalMatch(Text);
	while (It.hasNext())
	{
		const QString Entry = NormalizeScriptAssetPath(It.next().captured(1));
		if (!Entry.isEmpty())
			Tables << Entry;
	}
	return Tables;
}

enum LogMessageKind
{
	LogMessageDefault,
	LogMessageMuted,
	LogMessageCommand,
	LogMessageInfo,
	LogMessageSuccess,
	LogMessageWarning,
	LogMessageError,
	LogMessageLaunch,
	LogMessageAccent
};

static LogMessageKind DetectLogMessageKind(const QString& Output);

enum LogFilterKind
{
	LogFilterLeakMod,
	LogFilterPhilLibX,
	LogFilterGdtDb,
	LogFilterLinking,
	LogFilterCommands,
	LogFilterWarnings,
	LogFilterErrors
};

enum OutputLogTab
{
	OutputLogTabFull,
	OutputLogTabErrors,
	OutputLogTabWarnings,
	OutputLogTabMaterials,
	OutputLogTabImages,
	OutputLogTabXModels,
	OutputLogTabPlayerCharacters,
	OutputLogTabConvertingMesh,
	OutputLogTabTechsets,
	OutputLogTabShaders
};

static QString LogFilterSettingKey(LogFilterKind Kind)
{
	switch (Kind)
	{
	case LogFilterLeakMod:
		return "LogFilters/ShowLeakMod";
	case LogFilterPhilLibX:
		return "LogFilters/ShowPhilLibX";
	case LogFilterGdtDb:
		return "LogFilters/ShowGdtDb";
	case LogFilterLinking:
		return "LogFilters/ShowLinking";
	case LogFilterCommands:
		return "LogFilters/ShowCommands";
	case LogFilterWarnings:
		return "LogFilters/ShowWarnings";
	case LogFilterErrors:
		return "LogFilters/ShowErrors";
	default:
		return QString();
	}
}

static bool MatchesLogFilterKind(LogFilterKind Kind, const QString& Output)
{
	const QString Text = Output.trimmed();
	const QString Lower = Text.toLower();

	switch (Kind)
	{
	case LogFilterLeakMod:
		return Lower.contains("l3akmod") || Lower.contains("leakmod");
	case LogFilterPhilLibX:
		return Lower.contains("phillibx") || Lower.contains("philibx") || Lower.contains("philipx") || Lower.contains("philip") || Lower.contains("t7mtenhancements");
	case LogFilterGdtDb:
		return Lower.startsWith("gdtdb:");
	case LogFilterLinking:
		return Lower.startsWith("linking ") || Lower == "processing..." || Lower.startsWith("done:");
	case LogFilterCommands:
		return DetectLogMessageKind(Text) == LogMessageCommand || DetectLogMessageKind(Text) == LogMessageLaunch;
	case LogFilterWarnings:
		return DetectLogMessageKind(Text) == LogMessageWarning;
	case LogFilterErrors:
		return DetectLogMessageKind(Text) == LogMessageError;
	default:
		return false;
	}
}

static bool IsLogFilterEnabled(const QSettings& Settings, LogFilterKind Kind)
{
	const QString Key = LogFilterSettingKey(Kind);
	return Key.isEmpty() ? true : Settings.value(Key, true).toBool();
}

static bool MatchesOutputLogTab(int TabIndex, const QString& Output)
{
	const QString Text = Output.trimmed();
	const QString Lower = Text.toLower();

	switch (TabIndex)
	{
	case OutputLogTabFull:
		return true;
	case OutputLogTabErrors:
		return DetectLogMessageKind(Text) == LogMessageError || Lower.startsWith("error:") || Lower.contains(" failed");
	case OutputLogTabWarnings:
		return DetectLogMessageKind(Text) == LogMessageWarning || Lower.contains("warning");
	case OutputLogTabMaterials:
		return Lower.startsWith("material:") || Lower.startsWith("material_draw_method:");
	case OutputLogTabImages:
		return Lower.startsWith("image:") || Lower.startsWith("images:");
	case OutputLogTabXModels:
		return Lower.startsWith("xmodel:");
	case OutputLogTabPlayerCharacters:
		return Lower.startsWith("playerbodystyle:") || Lower.startsWith("playerbodytype:") || Lower.startsWith("customizationtable:") || Lower.startsWith("character:");
	case OutputLogTabConvertingMesh:
		return Lower.contains("converting") || Lower.contains("mesh") || Lower.contains("xmodel_export") || Lower.contains("xanim_export");
	case OutputLogTabTechsets:
		return Lower.startsWith("techset:");
	case OutputLogTabShaders:
		return Lower.contains("shader") || Lower.contains("technique(") || Lower.contains("gettechnique(");
	default:
		return true;
	}
}

static bool ShouldDisplayLogOutput(const QSettings& Settings, const QString& Output, int OutputTabIndex = OutputLogTabFull)
{
	if (!MatchesOutputLogTab(OutputTabIndex, Output))
		return false;

	const LogFilterKind FilterOrder[] =
	{
		LogFilterLeakMod,
		LogFilterPhilLibX,
		LogFilterGdtDb,
		LogFilterLinking,
		LogFilterCommands,
		LogFilterWarnings,
		LogFilterErrors
	};

	for (int FilterIdx = 0; FilterIdx < static_cast<int>(sizeof(FilterOrder) / sizeof(FilterOrder[0])); FilterIdx++)
	{
		const LogFilterKind Kind = FilterOrder[FilterIdx];
		if (MatchesLogFilterKind(Kind, Output))
			return IsLogFilterEnabled(Settings, Kind);
	}

	return true;
}

static bool IsLogBlockHeader(const QString& Text)
{
	const QString Lower = Text.trimmed().toLower();
	if (Lower.isEmpty())
		return false;

	return Lower.startsWith("gdtdb:")
		|| Lower.startsWith("[l3akmod")
		|| Lower.startsWith("phillibx")
		|| Lower.startsWith("philibx")
		|| Lower.startsWith("philipx")
		|| Lower.startsWith("philip")
		|| Lower.startsWith("linking ")
		|| Lower.startsWith("^1material ")
		|| Lower.startsWith("<error:")
		|| Lower.contains("linker_modtools.exe")
		|| Lower.contains("cod2map64.exe")
		|| Lower.contains("radiant_modtools.exe")
		|| Lower.contains("blackops3.exe")
		|| Lower.contains(": error x")
		|| Lower.contains(": warning x");
}

static bool IsUnrecoverableLogLine(const QString& Text)
{
	const QString Trimmed = Text.trimmed();
	const QString Lower = Trimmed.toLower();
	if (Trimmed.isEmpty())
		return false;

	bool AllStars = true;
	for (int CharIdx = 0; CharIdx < Trimmed.length(); CharIdx++)
	{
		if (Trimmed[CharIdx] != '*')
		{
			AllStars = false;
			break;
		}
	}

	return AllStars
		|| Lower == "unrecoverable error:"
		|| Lower.contains("one or more shaders failed to compile")
		|| Lower == "linker will now terminate.";
}

static bool ShouldContinueCurrentLogBlock(const QString& CurrentBlockText, const QString& NextLine)
{
	const QString CurrentLower = CurrentBlockText.toLower();
	const QString CurrentFirstLineLower = CurrentBlockText.section('\n', 0, 0).trimmed().toLower();
	const QString NextLower = NextLine.trimmed().toLower();
	if (CurrentLower.isEmpty() || NextLower.isEmpty())
		return false;

	const bool GdtDbBlock = CurrentLower.contains("gdtdb.exe") || CurrentLower.contains("gdtdb:");
	if (GdtDbBlock)
	{
		return NextLower.startsWith("gdtdb:")
			|| NextLower.startsWith("processed ")
			|| NextLower.startsWith("processed(")
			|| NextLower.startsWith("gdtdb: successfully updated database");
	}

	const bool LinkingProcessingBlock = CurrentFirstLineLower.startsWith("linking ") && CurrentLower.contains("processing...");
	if (LinkingProcessingBlock && NextLower.startsWith("done:"))
		return true;

	const bool L3akModBlock = CurrentFirstLineLower.startsWith("[l3akmod");
	if (L3akModBlock && NextLower.startsWith("[l3akmod"))
		return true;

	const bool PhilLibXBlock = CurrentFirstLineLower.startsWith("phillibx.t7mtenhancements:");
	if (PhilLibXBlock && NextLower.startsWith("phillibx.t7mtenhancements:"))
		return true;

	return false;
}

static int LogDetailIndentLevel(const QString& Text);

static QString RenderedLogLine(const QString& RawLine)
{
	const QString TrimmedLine = RawLine.trimmed();
	if (TrimmedLine == RawLine && LogDetailIndentLevel(RawLine) > 0)
		return QString(LogDetailIndentLevel(RawLine) * 4, ' ') + TrimmedLine;
	return RawLine;
}

static QStringList ExtractLogBlocks(const QString& Output)
{
	QStringList Blocks;
	QString CurrentBlockText;
	const QString NormalizedOutput = QString(Output).replace("\r", "");
	const QStringList OutputLines = NormalizedOutput.split('\n');

	for (const QString& RawLine : OutputLines)
	{
		const QString TrimmedLine = RawLine.trimmed();
		if (TrimmedLine.isEmpty())
			continue;

		const bool CurrentBlockIsUnrecoverable = !CurrentBlockText.isEmpty() && IsUnrecoverableLogLine(CurrentBlockText);
		const bool ContinueUnrecoverableBlock = CurrentBlockIsUnrecoverable && IsUnrecoverableLogLine(TrimmedLine);
		const bool ContinueCurrentBlock = !CurrentBlockText.isEmpty() && ShouldContinueCurrentLogBlock(CurrentBlockText, TrimmedLine);
		const bool StartsNewBlock = !ContinueUnrecoverableBlock && !ContinueCurrentBlock && (CurrentBlockText.isEmpty() || IsLogBlockHeader(TrimmedLine) || LogDetailIndentLevel(RawLine) <= 0);

		if (StartsNewBlock)
		{
			if (!CurrentBlockText.isEmpty())
				Blocks.append(CurrentBlockText);
			CurrentBlockText = TrimmedLine;
			continue;
		}

		if (!CurrentBlockText.isEmpty())
			CurrentBlockText += "\n";
		CurrentBlockText += RenderedLogLine(RawLine);
	}

	if (!CurrentBlockText.isEmpty())
		Blocks.append(CurrentBlockText);

	return Blocks;
}

static QString BuildPlainConsoleOutput(const QString& Output, bool CompactBlankLines, int OutputTabIndex = OutputLogTabFull)
{
	const QString NormalizedOutput = QString(Output).replace("\r", "");
	QSettings Settings;
	QStringList VisibleLines;
	const QStringList RawLines = NormalizedOutput.split('\n');
	VisibleLines.reserve(RawLines.count());
	for (const QString& Line : RawLines)
	{
		const QString TrimmedLine = Line.trimmed();
		if (TrimmedLine.isEmpty() || ShouldDisplayLogOutput(Settings, TrimmedLine, OutputTabIndex))
			VisibleLines.append(Line);
	}

	const QString FilteredOutput = VisibleLines.join("\n");
	if (!CompactBlankLines)
		return FilteredOutput;

	const QStringList Blocks = ExtractLogBlocks(FilteredOutput);
	if (!Blocks.isEmpty())
	{
		QStringList VisibleBlocks;
		for (const QString& Block : Blocks)
		{
			bool ShowBlock = false;
			const QStringList BlockLines = Block.split('\n');
			for (const QString& BlockLine : BlockLines)
			{
				const QString TrimmedLine = BlockLine.trimmed();
				if (!TrimmedLine.isEmpty() && ShouldDisplayLogOutput(Settings, TrimmedLine, OutputTabIndex))
				{
					ShowBlock = true;
					break;
				}
			}
			if (ShowBlock)
				VisibleBlocks.append(Block);
		}

		if (!VisibleBlocks.isEmpty())
			return VisibleBlocks.join("\n\n");
	}

	const QStringList Lines = FilteredOutput.split('\n');
	QStringList CompactLines;
	CompactLines.reserve(Lines.count());
	bool PreviousLineBlank = false;
	for (const QString& Line : Lines)
	{
		const bool IsBlank = Line.trimmed().isEmpty();
		if (IsBlank)
		{
			if (PreviousLineBlank)
				continue;
			PreviousLineBlank = true;
			CompactLines.append(QString());
			continue;
		}

		PreviousLineBlank = false;
		CompactLines.append(Line);
	}

	return CompactLines.join("\n");
}

static bool IsConsoleColorCodingEnabled(const QSettings& Settings)
{
	return Settings.value("ConsoleColorCodingEnabled", true).toBool();
}

static QString LogBlockLineColor(const QString& Line, const QString& DefaultColor);

class OutputPlainHighlighter : public QSyntaxHighlighter
{
public:
	OutputPlainHighlighter(QTextDocument* Parent)
		: QSyntaxHighlighter(Parent), mColorize(false), mDefaultColor("#d7dce2")
	{
	}

	void Configure(bool Colorize, const QColor& DefaultColor)
	{
		mColorize = Colorize;
		if (DefaultColor.isValid())
			mDefaultColor = DefaultColor;
		rehighlight();
	}

protected:
	void highlightBlock(const QString& Text)
	{
		if (Text.isEmpty())
			return;

		if (!mColorize)
		{
			setFormat(0, Text.length(), QTextCharFormat());
			return;
		}

		QTextCharFormat LineFormat;
		LineFormat.setForeground(QColor(LogBlockLineColor(Text, mDefaultColor.name(QColor::HexRgb))));
		setFormat(0, Text.length(), LineFormat);
	}

private:
	bool mColorize;
	QColor mDefaultColor;
};

static void EnsureOutputPlainHighlighter(QPlainTextEdit* PlainOutput, bool Colorize, const QColor& DefaultColor)
{
	if (!PlainOutput || !PlainOutput->document())
		return;

	OutputPlainHighlighter* Highlighter = PlainOutput->document()->findChild<OutputPlainHighlighter*>("OutputPlainHighlighter");
	if (!Highlighter)
	{
		Highlighter = new OutputPlainHighlighter(PlainOutput->document());
		Highlighter->setObjectName("OutputPlainHighlighter");
	}

	Highlighter->Configure(Colorize, DefaultColor);
}

static bool ShouldDisplayLogBlock(const QSettings& Settings, const QString& BlockText, int OutputTabIndex)
{
	const QStringList Lines = BlockText.split('\n');
	for (const QString& Line : Lines)
	{
		const QString TrimmedLine = Line.trimmed();
		if (TrimmedLine.isEmpty())
			continue;
		if (ShouldDisplayLogOutput(Settings, TrimmedLine, OutputTabIndex))
			return true;
	}

	return false;
}

static bool IsScrollBarAtBottom(const QScrollBar* ScrollBar)
{
	return !ScrollBar || ScrollBar->value() >= ScrollBar->maximum();
}

static int LogDetailIndentLevel(const QString& Text)
{
	int LeadingSpaces = 0;
	while (LeadingSpaces < Text.length() && (Text[LeadingSpaces] == ' ' || Text[LeadingSpaces] == '\t'))
		LeadingSpaces++;

	if (LeadingSpaces > 0)
		return qMax(1, LeadingSpaces / 2);

	const QString Lower = Text.trimmed().toLower();
	if (Lower == "processing..." || Lower.startsWith("done:"))
		return 1;
	if (Lower.startsWith("material_draw_method") || Lower.startsWith("technique[") || Lower.startsWith("technique(") || Lower.startsWith("<error:"))
		return 1;
	if (Lower.startsWith("techset:") || Lower.startsWith("material:") || Lower.startsWith("csv:") || Lower.startsWith("weapon:") || Lower.startsWith("weaponcamo:"))
		return 2;
	return 0;
}

static QColor OutputBlockBackgroundColor(int ThemeModeValue, LogMessageKind Kind, int BlockIndex)
{
	const bool UseClassicChrome = ThemeUsesClassicChrome(ThemeModeValue);
	const bool UseDarkModernChrome = ThemeUsesDarkModernChrome(ThemeModeValue);
	Q_UNUSED(BlockIndex);
	QColor BaseColor;
	if (UseClassicChrome)
		BaseColor = QColor(54, 54, 54, 255);
	else if (UseDarkModernChrome)
		BaseColor = QColor(5, 6, 8, (BlockIndex % 2) ? 146 : 158);
	else
		BaseColor = QColor(10, 10, 10, (BlockIndex % 2) ? 132 : 146);

	if (Kind == LogMessageWarning)
		BaseColor = UseClassicChrome ? QColor(66, 58, 44, 255) : (UseDarkModernChrome ? QColor(30, 20, 9, 144) : QColor(34, 24, 12, 138));
	else if (Kind == LogMessageError)
		BaseColor = UseClassicChrome ? QColor(68, 48, 48, 255) : (UseDarkModernChrome ? QColor(30, 11, 11, 148) : QColor(34, 14, 14, 142));
	else if (Kind == LogMessageSuccess)
		BaseColor = UseClassicChrome ? QColor(46, 62, 52, 255) : (UseDarkModernChrome ? QColor(11, 26, 18, 140) : QColor(14, 28, 20, 134));
	else if (Kind == LogMessageAccent || Kind == LogMessageLaunch)
		BaseColor = UseClassicChrome ? QColor(58, 54, 70, 255) : (UseDarkModernChrome ? QColor(20, 15, 30, 142) : QColor(24, 18, 34, 136));

	return BaseColor;
}

static QString LogBlockText(const QTreeWidgetItem* Item)
{
	if (!Item)
		return QString();

	return Item->data(0, ML_LOG_TEXT_ROLE).toString();
}

static void ApplyLogRowVisualStyle(QWidget* RowWidget, const QColor& BackgroundColor, bool RoundTop, bool RoundBottom)
{
	if (!RowWidget)
		return;

	const int TopRadius = RoundTop ? 4 : 0;
	const int BottomRadius = RoundBottom ? 4 : 0;
	RowWidget->setStyleSheet(
		QString("background:%1; border-top-left-radius:%2px; border-top-right-radius:%2px; border-bottom-left-radius:%3px; border-bottom-right-radius:%3px;")
			.arg(BackgroundColor.name(QColor::HexArgb))
			.arg(TopRadius)
			.arg(BottomRadius));
}

static QWidget* CreateLogRowWidget(QTreeWidget* Parent, const QString& Text, const QColor& TextColor, const QColor& BackgroundColor, int IndentLevel, bool IsHeader, bool RoundTop = true, bool RoundBottom = true)
{
	QWidget* RowWidget = new QWidget(Parent);
	RowWidget->setObjectName(IsHeader ? "LogHeaderRow" : "LogDetailRow");
	QHBoxLayout* Layout = new QHBoxLayout(RowWidget);
	Layout->setContentsMargins(8 + (IndentLevel * 16), IsHeader ? 5 : 1, 8, IsHeader ? 5 : 1);
	Layout->setSpacing(0);

	QLabel* Label = new QLabel(Text, RowWidget);
	Label->setTextFormat(Qt::PlainText);
	Label->setWordWrap(false);
	Label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	Label->setCursor(Qt::IBeamCursor);
	Label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	QFont FixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	FixedFont.setPointSize(qMax(8, FixedFont.pointSize()));
	Label->setFont(FixedFont);
	Label->setStyleSheet(QString("background: transparent; color: %1;").arg(TextColor.name(QColor::HexRgb)));
	Layout->addWidget(Label, 1);

	ApplyLogRowVisualStyle(RowWidget, BackgroundColor, RoundTop, RoundBottom);
	return RowWidget;
}

static QString DisplayedLogBlockText(const QString& StoredText, bool Expanded)
{
	if (Expanded)
		return StoredText;

	const int NewlineIdx = StoredText.indexOf('\n');
	return NewlineIdx >= 0 ? StoredText.left(NewlineIdx) : StoredText;
}

static QFont LogBlockFont()
{
	QFont FixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	FixedFont.setPointSize(qMax(8, FixedFont.pointSize()));
	return FixedFont;
}

static int LogBlockWidgetHeight(const QString& Text, bool Expanded)
{
	const QString DisplayText = DisplayedLogBlockText(Text, Expanded);
	const int LineCount = qMax(1, DisplayText.count('\n') + 1);
	return (QFontMetrics(LogBlockFont()).lineSpacing() * LineCount) + 18;
}

static QString LogBlockLineColor(const QString& Line, const QString& DefaultColor)
{
	QSettings Settings;
	if (!IsConsoleColorCodingEnabled(Settings))
		return DefaultColor;

	const QString TrimmedLower = Line.trimmed().toLower();
	if (TrimmedLower.contains("^3"))
		return QString("#f6bd60");
	if (TrimmedLower.startsWith("^1"))
		return QString("#ff7a7a");
	if (TrimmedLower.startsWith("weapon:") || TrimmedLower.startsWith("weaponcamo:"))
		return QString("#c792ea");
	if (TrimmedLower.contains("material '") && TrimmedLower.contains("using technique"))
		return QString("#f6bd60");
	if (TrimmedLower.startsWith("material textures:"))
		return QString("#f6bd60");
	if (TrimmedLower.contains("linker_modtools.exe"))
		return QString("#8fb7ff");
	if (TrimmedLower.startsWith("processed ") || TrimmedLower.startsWith("processed("))
		return QString("#8fb7ff");
	if (TrimmedLower.startsWith("gdtdb:") || TrimmedLower.contains("gdtdb"))
		return QString("#8fb7ff");
	if (TrimmedLower.startsWith("[l3akmod"))
		return QString("#88dcb7");
	if (TrimmedLower.startsWith("phillibx.t7mtenhancements:"))
		return QString("#789b98");
	if (TrimmedLower.startsWith("linking ") || TrimmedLower == "processing..." || TrimmedLower.startsWith("done:"))
		return QString("#6ee7a8");
	if (TrimmedLower.startsWith("techset:"))
		return QString("#c792ea");
	if (TrimmedLower.startsWith("material:"))
		return QString("#6cb6ff");
	if (TrimmedLower.startsWith("material_draw_method:"))
		return QString("#f6bd60");
	if (TrimmedLower.startsWith("xmodel:"))
		return QString("#4fd1c5");
	if (TrimmedLower.startsWith("playerbodystyle:"))
		return QString("#7ee787");
	if (TrimmedLower.startsWith("playerbodytype:"))
		return QString("#9be9a8");
	if (TrimmedLower.startsWith("customizationtable:"))
		return QString("#ffd866");
	if (TrimmedLower.startsWith("csv:"))
		return QString("#8b949e");
	if (TrimmedLower.startsWith("error:") || TrimmedLower.contains(" error:") || TrimmedLower.contains(" failed"))
		return QString("#ff7a7a");
	return DefaultColor;
}

static QString LogBlockHtml(const QString& Text, const QColor& TextColor, bool Expanded)
{
	const QString DisplayText = DisplayedLogBlockText(Text, Expanded);
	const QString DefaultColor = TextColor.name(QColor::HexRgb);
	const QString ErrorColor = QString("#ff7a7a");
	QStringList HtmlLines;
	const QStringList Lines = DisplayText.split('\n');

	for (QString Line : Lines)
	{
		Line.replace(QRegularExpression("\\^1(?=\\s*\\d+:|ERROR\\b)"), QString());
		const QString LineColor = LogBlockLineColor(Line, DefaultColor);

		QString EscapedLine = Line.toHtmlEscaped();
		EscapedLine.replace(" ", "&nbsp;");
		EscapedLine.replace("\t", "&nbsp;&nbsp;&nbsp;&nbsp;");
		EscapedLine.replace(QRegularExpression("ERROR"), QString("<span style=\"color:%1; font-weight:700;\">ERROR</span>").arg(ErrorColor));
		HtmlLines.append(QString("<span style=\"color:%1;\">%2</span>").arg(LineColor, EscapedLine));
	}

	return QString("<div style=\"font-family:'%1'; font-size:%2pt; white-space:nowrap;\">%3</div>")
		.arg(LogBlockFont().family())
		.arg(LogBlockFont().pointSize())
		.arg(HtmlLines.join("<br/>"));
}

static void UpdateLogBlockWidget(QWidget* BlockWidget, const QString& Text, const QColor& TextColor, const QColor& BackgroundColor, bool Expanded);

static QWidget* CreateLogBlockWidget(QWidget* Parent, const QString& Text, const QColor& TextColor, const QColor& BackgroundColor, bool Expanded)
{
	QWidget* BlockWidget = new QWidget(Parent);
	BlockWidget->setObjectName("LogBlockWidget");
	BlockWidget->setFocusPolicy(Qt::ClickFocus);
	QVBoxLayout* Layout = new QVBoxLayout(BlockWidget);
	Layout->setContentsMargins(8, 5, 8, 5);
	Layout->setSpacing(0);

	QLabel* BlockLabel = new QLabel(BlockWidget);
	BlockLabel->setObjectName("LogBlockText");
	BlockLabel->setFocusPolicy(Qt::ClickFocus);
	BlockLabel->setTextFormat(Qt::RichText);
	BlockLabel->setWordWrap(false);
	BlockLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	BlockLabel->setCursor(Qt::IBeamCursor);
	BlockLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	BlockLabel->setFont(LogBlockFont());
	BlockLabel->setText(LogBlockHtml(Text, TextColor, Expanded));
	BlockLabel->setStyleSheet(QString("background: transparent; color: %1;").arg(TextColor.name(QColor::HexRgb)));
	BlockLabel->setFixedHeight(LogBlockWidgetHeight(Text, Expanded) - 10);
	Layout->addWidget(BlockLabel);
	BlockWidget->setFixedHeight(LogBlockWidgetHeight(Text, Expanded));

	ApplyLogRowVisualStyle(BlockWidget, BackgroundColor, true, true);
	return BlockWidget;
}

static QWidget* CreateLogBlockActionWidget(QTreeWidget* ParentTree, QTreeWidgetItem* Item)
{
	QWidget* ActionsWidget = new QWidget(ParentTree);
	ActionsWidget->setObjectName("LogBlockActionWidget");
	QHBoxLayout* ActionsLayout = new QHBoxLayout(ActionsWidget);
	ActionsLayout->setContentsMargins(0, 0, 0, 0);
	ActionsLayout->setSpacing(4);

	QToolButton* CopyButton = new QToolButton(ActionsWidget);
	CopyButton->setObjectName("LogBlockCopyButton");
	QIcon CopyIcon = QIcon::fromTheme("edit-copy");
	if (CopyIcon.isNull())
		CopyIcon = QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton);
	CopyButton->setIcon(CopyIcon);
	CopyButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	CopyButton->setFixedSize(20, 18);
	CopyButton->setAutoRaise(false);
	CopyButton->setToolTip("Copy this block to clipboard.");
	CopyButton->setStyleSheet("QToolButton { background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.18); border-radius: 3px; padding: 0; }"
		"QToolButton:hover { background: rgba(255,255,255,0.26); border: 1px solid rgba(255,255,255,0.50); }"
		"QToolButton:pressed { background: rgba(255,255,255,0.36); border: 1px solid rgba(255,255,255,0.65); }");
	QObject::connect(CopyButton, &QToolButton::clicked, ActionsWidget, [Item]()
	{
		if (!Item)
			return;
		QApplication::clipboard()->setText(Item->data(0, ML_LOG_TEXT_ROLE).toString());
	});

	QToolButton* ToggleButton = new QToolButton(ActionsWidget);
	ToggleButton->setObjectName("LogBlockToggleButton");
	ToggleButton->setAutoRaise(true);
	ToggleButton->setToolTip("Collapse or expand this block.");
	ToggleButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	ToggleButton->setFixedSize(18, 18);
	ToggleButton->setStyleSheet("QToolButton { background: transparent; border: 0; padding: 0; }"
		"QToolButton:hover { background: transparent; border: 0; }"
		"QToolButton:pressed { background: transparent; border: 0; }");
	const bool Expanded = Item && Item->data(0, ML_LOG_EXPANDED_ROLE).toBool();
	ToggleButton->setArrowType(Expanded ? Qt::DownArrow : Qt::RightArrow);
	QObject::connect(ToggleButton, &QToolButton::clicked, ActionsWidget, [ParentTree, Item]()
	{
		if (!ParentTree || !Item)
			return;

		const bool ExpandedNow = !Item->data(0, ML_LOG_EXPANDED_ROLE).toBool();
		Item->setData(0, ML_LOG_EXPANDED_ROLE, ExpandedNow);
		Item->setExpanded(ExpandedNow);
		const QString BlockText = Item->data(0, ML_LOG_TEXT_ROLE).toString();
		Item->setSizeHint(0, QSize(0, LogBlockWidgetHeight(BlockText, ExpandedNow)));
		const QColor TextColor = Item->data(0, ML_LOG_TEXT_COLOR_ROLE).value<QColor>();
		const QColor BackgroundColor = Item->data(0, ML_LOG_BG_COLOR_ROLE).value<QColor>();
		UpdateLogBlockWidget(ParentTree->itemWidget(Item, 0), BlockText, TextColor, BackgroundColor, ExpandedNow);
		if (QWidget* ActionWidget = ParentTree->itemWidget(Item, 1))
		{
			QToolButton* ActionToggleButton = ActionWidget->findChild<QToolButton*>("LogBlockToggleButton");
			if (ActionToggleButton)
				ActionToggleButton->setArrowType(ExpandedNow ? Qt::DownArrow : Qt::RightArrow);
		}
	});

	ActionsLayout->addWidget(CopyButton);
	ActionsLayout->addWidget(ToggleButton);
	ActionsLayout->addStretch(1);
	return ActionsWidget;
}

static void UpdateLogBlockWidget(QWidget* BlockWidget, const QString& Text, const QColor& TextColor, const QColor& BackgroundColor, bool Expanded)
{
	if (!BlockWidget)
		return;

	QLabel* BlockLabel = BlockWidget->findChild<QLabel*>("LogBlockText");
	if (!BlockLabel)
		return;

	BlockLabel->setText(LogBlockHtml(Text, TextColor, Expanded));
	BlockLabel->setFixedHeight(LogBlockWidgetHeight(Text, Expanded) - 10);
	BlockLabel->setStyleSheet(QString("background: transparent; color: %1;").arg(TextColor.name(QColor::HexRgb)));
	BlockWidget->setFixedHeight(LogBlockWidgetHeight(Text, Expanded));
	ApplyLogRowVisualStyle(BlockWidget, BackgroundColor, true, true);
}

static bool UseImprovedConsoleStyle(const QSettings& Settings)
{
	return Settings.value("ConsoleStyle", "improved").toString().compare("original", Qt::CaseInsensitive) != 0;
}

static LogMessageKind DetectLogMessageKind(const QString& Output)
{
	const QString Text = Output.trimmed();
	const QString Lower = Text.toLower();

	if (Lower.isEmpty())
		return LogMessageDefault;

	if (Lower.contains("leakmod"))
		return LogMessageMuted;

	if (Lower.contains("phillibx") || Lower.contains("philibx") || Lower.contains("philipx") || Lower.contains("philip") || Lower.contains("t7mtenhancements"))
		return LogMessageAccent;

	if (Lower.startsWith("linking ") && Lower.contains("done:") && !Lower.contains("warning") && !Lower.contains("error") && !Lower.contains("failed"))
		return LogMessageSuccess;

	if (Lower.startsWith("linking "))
		return LogMessageWarning;

	if (Lower.startsWith("done:"))
		return LogMessageSuccess;

	if (Text.startsWith("^1") || Lower.contains("mismatched usage") || Lower.contains("doesn't expose") || Lower.contains("material "))
		return LogMessageWarning;

	if (Lower.contains("blackops3.exe") || Lower.contains("+devmap"))
		return LogMessageLaunch;

	if (Lower.contains(".exe ") || Lower.endsWith(".exe") || Lower.contains("linker_modtools") || Lower.contains("cod2map64") || Lower.contains("radiant_modtools"))
		return LogMessageCommand;

	if (Lower.contains("error") || Lower.contains("failed") || Lower.contains("could not") || Lower.contains("abnormally") || Lower.contains("invalid"))
		return LogMessageError;

	if (Lower.contains("warning") || Lower.contains("skipping") || Lower.contains("english-only") || Lower.contains("english only"))
		return LogMessageWarning;

	if (Lower.contains("success") || Lower.contains("saved") || Lower.contains("created") || Lower.contains("complete") || Lower.contains("updated"))
		return LogMessageSuccess;

	if (Lower.contains("uploading") || Lower.contains("preparing") || Lower.contains("committing") || Lower.contains("converting"))
		return LogMessageInfo;

	return LogMessageDefault;
}

static QColor ColorForLogMessageKind(const QSettings& Settings, LogMessageKind Kind)
{
	switch (Kind)
	{
	case LogMessageMuted:
		return QColor(Settings.value("LogColors/Muted", "#8b929a").toString());
	case LogMessageCommand:
		return QColor(Settings.value("LogColors/Command", "#7dcfff").toString());
	case LogMessageInfo:
		return QColor(Settings.value("LogColors/Info", "#eef1f4").toString());
	case LogMessageSuccess:
		return QColor(Settings.value("LogColors/Success", "#6ee7a8").toString());
	case LogMessageWarning:
		return QColor(Settings.value("LogColors/Warning", "#ffcf70").toString());
	case LogMessageError:
		return QColor(Settings.value("LogColors/Error", "#ff7a7a").toString());
	case LogMessageLaunch:
		return QColor(Settings.value("LogColors/Launch", "#c792ea").toString());
	case LogMessageAccent:
		return QColor(Settings.value("LogColors/Accent", "#c792ea").toString());
	default:
		return QColor(Settings.value("LogColors/Default", "#d7dce2").toString());
	}
}

static QString PrepareBackgroundImageCache(const QString& CacheRoot, const QString& SourcePath, int OpacityPercent)
{
	const QString NormalizedPath = SourcePath.trimmed();
	if (NormalizedPath.isEmpty())
		return QString();

	const bool IsQtResource = NormalizedPath.startsWith(":/") || NormalizedPath.startsWith("qrc:/");
	if (!IsQtResource && !QFileInfo(NormalizedPath).isFile())
		return QString();

	const QFileInfo Info(NormalizedPath);
	const QString CacheSourceKey = IsQtResource
		? NormalizedPath
		: QString("%1|%2|%3")
			.arg(QDir::toNativeSeparators(Info.absoluteFilePath()))
			.arg(Info.lastModified().toMSecsSinceEpoch())
			.arg(Info.size());
	const QByteArray CacheKey = QCryptographicHash::hash(
		QString("%1|%2").arg(CacheSourceKey).arg(qBound(0, OpacityPercent, 100)).toUtf8(),
		QCryptographicHash::Md5).toHex();
	const QString CacheDir = QDir::cleanPath(CacheRoot + "/background_cache");
	const QString CachePath = QDir::cleanPath(QString("%1/%2.png").arg(CacheDir, QString(CacheKey)));
	if (QFileInfo(CachePath).isFile())
		return CachePath;

	QPixmap SourcePixmap(NormalizedPath);
	if (SourcePixmap.isNull())
		return QString();

	QDir().mkpath(CacheDir);
	QPixmap OutputPixmap(SourcePixmap.size());
	OutputPixmap.fill(Qt::transparent);
	QPainter Painter(&OutputPixmap);
	Painter.setOpacity(qBound(0, OpacityPercent, 100) / 100.0);
	Painter.drawPixmap(0, 0, SourcePixmap);
	Painter.end();
	OutputPixmap.save(CachePath, "PNG");
	return CachePath;
}

static BOOL CALLBACK FindVisibleWindowForProcess(HWND WindowHandle, LPARAM Param)
{
	GameWindowSearchState* State = reinterpret_cast<GameWindowSearchState*>(Param);
	DWORD WindowProcessId = 0;
	GetWindowThreadProcessId(WindowHandle, &WindowProcessId);
	if (WindowProcessId != State->ProcessId || !IsWindowVisible(WindowHandle) || GetWindow(WindowHandle, GW_OWNER) != NULL)
		return TRUE;

	State->HasVisibleWindow = true;
	return FALSE;
}

mlBuildThread::mlBuildThread(const QList<QPair<QString, QStringList>>& Commands, bool IgnoreErrors)
	: mCommands(Commands), mSuccess(false), mCancel(false), mIgnoreErrors(IgnoreErrors), mActiveProcessId(0)
{
}

void mlBuildThread::run()
{
	bool Success = true;

	for (const QPair<QString, QStringList>& Command : mCommands)
	{
		if (IsGameLaunchCommand(Command.first, Command.second))
		{
			emit OutputReady(Command.first + ' ' + Command.second.join(' ') + "\n");
			QProcess::startDetached(Command.first, Command.second, QFileInfo(Command.first).absolutePath());
			emit CommandFinished(Command.first, Command.second, 0, 0);
			continue;
		}

		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
		Process->setWorkingDirectory(QFileInfo(Command.first).absolutePath());
		Process->setProcessChannelMode(QProcess::MergedChannels);
		QElapsedTimer CommandTimer;

		emit OutputReady(Command.first + ' ' + Command.second.join(' ') + "\n");

		CommandTimer.start();
		Process->start(Command.first, Command.second);
		if (!Process->waitForStarted(5000))
			return;
		mActiveProcessId = Process->processId();
		for (;;)
		{
			Sleep(100);

			if (Process->waitForReadyRead(0))
				emit OutputReady(Process->readAll());

			QProcess::ProcessState State = Process->state();
			if (State == QProcess::NotRunning)
				break;

			if (mCancel)
				Process->kill();
		}
		mActiveProcessId = 0;

		if (Process->exitStatus() != QProcess::NormalExit)
			return;

		emit CommandFinished(Command.first, Command.second, CommandTimer.elapsed(), Process->exitCode());

		if (Process->exitCode() != 0)
		{
			Success = false;
			if (!mIgnoreErrors)
				return;
		}
	}

	mActiveProcessId = 0;
	mSuccess = Success;
}

mlConvertThread::mlConvertThread(QStringList& Files, QString& OutputDir, bool IgnoreErrors, bool Overwrite)
	: mFiles(Files), mOutputDir(OutputDir), mOverwrite(Overwrite), mSuccess(false), mCancel(false), mIgnoreErrors(IgnoreErrors), mActiveProcessId(0)
{
}

void mlConvertThread::run()
{
	bool Success = true;
	unsigned int convCountSuccess = 0;
	unsigned int convCountSkipped = 0;
	unsigned int convCountFailed = 0;

	for (QString file : mFiles)
	{
		QFileInfo file_info(file);
		QString working_directory = file_info.absolutePath();

		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
		Process->setWorkingDirectory(working_directory);
		Process->setProcessChannelMode(QProcess::MergedChannels);

		file = file_info.baseName();

		QString ToolsPath = QDir::fromNativeSeparators(getenv("TA_TOOLS_PATH"));
		QString ExecutablePath = QString("%1bin/export2bin.exe").arg(ToolsPath);

		QStringList args;
		args.append("/piped");

		QString filepath = file_info.absoluteFilePath();

		QString ext = file_info.suffix().toUpper();
		if (ext == "XANIM_EXPORT")
			ext = ".XANIM_BIN";
		else if (ext == "XMODEL_EXPORT")
			ext = ".XMODEL_BIN";
		else
		{
			emit OutputReady("Export2Bin: Skipping file '" + filepath + "' (file has invalid extension)\n");
			convCountSkipped++;
			continue;
		}

		QString target_filepath = QDir::cleanPath(mOutputDir) + QDir::separator() + file + ext;

		QFile infile(filepath);
		QFile outfile(target_filepath);

		if (!mOverwrite && outfile.exists())
		{
			emit OutputReady("Export2Bin: Skipping file '" + filepath + "' (file already exists)\n");
			convCountSkipped++;
			continue;
		}

		infile.open(QIODevice::OpenMode::enum_type::ReadOnly);
		if (!infile.isOpen())
		{
			emit OutputReady("Export2Bin: Could not open '" + filepath + "' for reading\n");
			convCountFailed++;
			continue;
		}

		emit OutputReady("Export2Bin: Converting '" + file + "'");

		QByteArray buf = infile.readAll();
		infile.close();

		Process->start(ExecutablePath, args);
		if (!Process->waitForStarted(5000))
		{
			convCountFailed++;
			continue;
		}
		mActiveProcessId = Process->processId();
		Process->write(buf);
		Process->closeWriteChannel();

		QByteArray standardOutputPipeData;
		QByteArray standardErrorPipeData;

		for (;;)
		{
			Sleep(20);
			if (Process->waitForReadyRead(0))
			{
				standardOutputPipeData.append(Process->readAllStandardOutput());
				standardErrorPipeData.append(Process->readAllStandardError());
			}

			QProcess::ProcessState State = Process->state();
			if (State == QProcess::NotRunning)
				break;

			if (mCancel)
				Process->kill();
		}
		mActiveProcessId = 0;

		if (Process->exitStatus() != QProcess::NormalExit)
		{
			emit OutputReady("ERROR: Process exited abnormally");
			Success = false;
			break;
		}

		if (Process->exitCode() != 0)
		{
			emit OutputReady(standardOutputPipeData);
			emit OutputReady(standardErrorPipeData);
			convCountFailed++;
			if (!mIgnoreErrors)
			{
				Success = false;
				break;
			}
			continue;
		}

		outfile.open(QIODevice::OpenMode::enum_type::WriteOnly);
		if (!outfile.isOpen())
		{
			emit OutputReady("Export2Bin: Could not open '" + target_filepath + "' for writing\n");
			continue;
		}

		outfile.write(standardOutputPipeData);
		outfile.close();
		convCountSuccess++;
	}

	mActiveProcessId = 0;
	mSuccess = Success;
	if (mSuccess)
	{
		QString msg = QString("Export2Bin: Finished!\n\n"
			"Files Processed: %1\n"
			"Successes: %2\n"
			"Skipped: %3\n"
			"Failures: %4\n").arg(mFiles.count()).arg(convCountSuccess).arg(convCountSkipped).arg(convCountFailed);
		emit OutputReady(msg);
	}
}

mlMainWindow::mlMainWindow()
{
	AppendStartupTrace("ctor:start");
	QSettings Settings;
	EnsureThemeProfiles();
	AppendStartupTrace("ctor:settings-ready");

	mBuildThread = NULL;
	mConvertThread = NULL;
	mAnalyzeItemButton = NULL;
	mActiveBuildButton = NULL;
	mThemesButton = NULL;
	mActionEditAnalyze = NULL;
	mActionEditInformation = NULL;
	mActionFileOpenRoot = NULL;
	mActionFileOpenConsoleLog = NULL;
	mActionFileOpenConsoleLogExternal = NULL;
	mActionFileOpenScriptReference = NULL;
	mActionThemeModern = NULL;
	mActionThemeDarkModern = NULL;
	mActionThemeClassic = NULL;
	mCategoryTabs = NULL;
	mCategoryTabPencilButton = NULL;
	mAddCategoryTabButton = NULL;
	mCategoryTabHoveredIndex = -1;
	mCategorySummaryLabel = NULL;
	mOutputTabs = NULL;
	mCentralWidgetSplitter = NULL;
	mAssetDockWidget = NULL;
	mOutputDockWidget = NULL;
	mMainToolBar = NULL;
	mTopWidget = NULL;
	mLeftPanel = NULL;
	mActionsPanel = NULL;
	mOutputPanel = NULL;
	mTopLayout = NULL;
	mFileListWidget = NULL;
	mOutputWidget = NULL;
	mOutputPlainWidget = NULL;
	mAssetTreeBackgroundOverlay = NULL;
	mOutputBackgroundOverlay = NULL;
	mLogFiltersButton = NULL;
	mLogSelectionButton = NULL;
	mFooterVersionLabel = NULL;
	mFooterRefreshButton = NULL;
	mFooterUpdateStatusLabel = NULL;
	mFooterDownloadButton = NULL;
	mQuickLaunchLabel = NULL;
	mStartupQuoteLabel = NULL;
	mStartupQuoteText.clear();
	mStartupQuotePopupShown = false;
	mRunOnlineWidget = NULL;
	mCurrentOutputBlockItem = NULL;
	mOutputBlockCounter = 0;
	mCloseGameButton = NULL;
	mCloseGameStatusLabel = NULL;
	mLaunchProgressDialog = NULL;
	mUpdateNetworkAccess = new QNetworkAccessManager(this);
	mUpdateMetadataReply = NULL;
	mUpdateDownloadReply = NULL;
	mUpdateProgressDialog = NULL;
	mUpdateDownloadFile = NULL;
	mUpdateDownloadPath.clear();
	mPendingUpdateVersion.clear();
	mAvailableUpdateVersion.clear();
	mAvailableUpdateUrl.clear();
	mAvailableReleasePageUrl.clear();
	mLastStatsTickMs = 0;
	mLastActivityMs = 0;
	mCurrentItemStartedMs = 0;
	mPendingGameQueuedMs = 0;
	mCurrentGameStartedMs = 0;
	mPendingLaunchRequestedMs = 0;
	mCurrentStatsItemKey.clear();
	mPendingGameStatsItemKey.clear();
	mActiveGameStatsItemKey.clear();
	mCachedGameRunning = false;
	mGameProcessId = 0;
	mGameRunningState = GameNotRunning;
	mBuildLanguage = Settings.value("BuildLanguage", "english").toString();
	mLauncherLayout = Settings.value("LauncherLayout", "original").toString().trimmed().toLower();
	if (mLauncherLayout.isEmpty() || mLauncherLayout == "current")
		mLauncherLayout = "original";
	mThemeMode = ThemeModeFromSettings(Settings);
	mThemeProfileId = CurrentThemeProfileId();
	mLogBackgroundCachePath.clear();
	mAssetTreeBackgroundCachePath.clear();
	mPendingOutput.clear();
	mOutputFullText.clear();
	mOutputSelectionMode = true;
	mOutputTabIndex = OutputLogTabFull;
	mOutputTreeAutoFollow = true;
	mOutputPlainAutoFollow = true;
	mPendingOnlineLaunchFeedback = false;
	mPendingOnlineLaunchLabel.clear();
	mWorkshopUploadInFlight = false;
	mPostUploadSteamSyncPending = false;
	RefreshRunDvars();
	mOutputFlushTimer.setSingleShot(true);
	connect(&mOutputFlushTimer, &QTimer::timeout, this, &mlMainWindow::FlushBuildOutput);
	mStatsTimer.setInterval(1000);
	connect(&mStatsTimer, &QTimer::timeout, this, &mlMainWindow::StatsTick);
	mStatsElapsedTimer.start();
	IncrementStat("Stats/SessionCount");
	mStatsTimer.start();
	AppendStartupTrace("ctor:timers-ready");

	// Qt prefers '/' over '\\'
	mGamePath = QString(getenv("TA_GAME_PATH")).replace('\\', '/');
	mToolsPath = QString(getenv("TA_TOOLS_PATH")).replace('\\', '/');
	AppendStartupTrace(QString("ctor:paths game=%1 tools=%2").arg(mGamePath, mToolsPath));

	ApplyThemeProfile(mThemeProfileId);
	AppendStartupTrace("ctor:theme-profile-applied");

	setWindowIcon(QIcon(":/resources/ModLauncher.png"));
	setWindowTitle("BO3 Mod Tools Black");

	resize(1180, 780);

	CreateActions();
	AppendStartupTrace("ctor:actions-created");
	CreateMenu();
	AppendStartupTrace("ctor:menu-created");
	CreateToolBar();
	AppendStartupTrace("ctor:toolbar-created");

	mExport2BinGUIWidget = NULL;

	mTopWidget = new QWidget();
	mTopWidget->setObjectName("WorkspaceCentralWidget");

	mTopLayout = new QHBoxLayout(mTopWidget);
	mTopWidget->setLayout(mTopLayout);
	mTopLayout->setContentsMargins(8, 8, 8, 8);
	mTopLayout->setSpacing(10);

	mLeftPanel = new QWidget(this);
	mLeftPanel->setObjectName("AssetListPanel");
	QVBoxLayout* LeftLayout = new QVBoxLayout(mLeftPanel);
	LeftLayout->setContentsMargins(0, 0, 0, 0);
	LeftLayout->setSpacing(0);

	QWidget* CategoryHeader = new QWidget(mLeftPanel);
	QHBoxLayout* CategoryHeaderLayout = new QHBoxLayout(CategoryHeader);
	CategoryHeaderLayout->setContentsMargins(0, 0, 0, 0);
	CategoryHeaderLayout->setSpacing(8);
	mCategoryTabs = new CategoryTabBar(CategoryHeader);
	mCategoryTabs->setObjectName("CategoryTabs");
	mCategoryTabs->setMouseTracking(true);
	auto AddCategoryTab = [&](const QString& DefaultLabel, const QString& Key)
	{
		const QString OverrideName = Settings.value(QString("TabOverrides/%1/Name").arg(Key)).toString();
		const QString OverrideColor = Settings.value(QString("TabOverrides/%1/Color").arg(Key)).toString();
		const QString OverrideIcon = Settings.value(QString("TabOverrides/%1/Icon").arg(Key)).toString();
		const QString ActualLabel = BuildTabLabel(OverrideIcon, OverrideName.isEmpty() ? DefaultLabel : OverrideName);
		const int Index = mCategoryTabs->addTab(ActualLabel);
		mCategoryTabs->setTabIcon(Index, BuildTabIcon(OverrideIcon));
		mCategoryTabs->setTabData(Index, Key);
		if (!OverrideColor.isEmpty())
		{
			const QColor Col(OverrideColor);
			if (Col.isValid())
				mCategoryTabs->setTabTextColor(Index, Col);
		}
	};
	AddCategoryTab("All", "all");
	AddCategoryTab("Recent", "recent");
	AddCategoryTab("Favorites", "favorites");
	AddCategoryTab("ZM Maps", "zm-maps");
	AddCategoryTab("MP Maps", "mp-maps");
	AddCategoryTab("Mods", "mods");
	// Load saved custom tabs
	{
		const QList<QVariantMap> CustomTabDefs = LoadCustomTabDefs();
		for (const QVariantMap& Tab : CustomTabDefs)
		{
			const QString TabId = Tab.value("id").toString();
			const QString TabName = Tab.value("name").toString();
			const QString TabIconValue = Tab.value("icon").toString();
			const int Index = mCategoryTabs->addTab(BuildTabLabel(TabIconValue, TabName));
			mCategoryTabs->setTabIcon(Index, BuildTabIcon(TabIconValue));
			mCategoryTabs->setTabData(Index, TabId);
			if (!Tab.value("color").toString().isEmpty())
			{
				const QColor Col(Tab.value("color").toString());
				if (Col.isValid())
					mCategoryTabs->setTabTextColor(Index, Col);
			}
		}
	}
	const int AddTabIndex = mCategoryTabs->addTab("+");
	mCategoryTabs->setTabData(AddTabIndex, "__add-category-tab__");
	mCategoryTabs->setTabToolTip(AddTabIndex, "Add custom tab");
	mCategoryTabs->setDocumentMode(true);
	mCategoryTabs->setExpanding(false);
	mCategoryTabs->setDrawBase(false);
	mCategoryTabs->setUsesScrollButtons(false);
	mCategoryTabs->setElideMode(Qt::ElideNone);
	mCategoryTabs->setMovable(true);
	mCategoryTabs->setContextMenuPolicy(Qt::CustomContextMenu);
	const QString SavedCategoryTabKey = NormalizeCategoryTabKey(Settings.value("ActiveCategoryTabKey", "").toString());
	int SavedCategoryTab = FindCategoryTabIndex(mCategoryTabs, SavedCategoryTabKey);
	if (SavedCategoryTab < 0)
	{
		const int SavedCategoryTabIndex = Settings.value("ActiveCategoryTab", CategoryAll).toInt();
		SavedCategoryTab = qBound(0, SavedCategoryTabIndex, qMax(0, mCategoryTabs->count() - 2));
	}
	if (IsAddCategoryTabKey(mCategoryTabs->tabData(SavedCategoryTab).toString()))
		SavedCategoryTab = 0;
	mCategoryTabs->setCurrentIndex(SavedCategoryTab);
	mCategoryTabPencilButton = NULL;
	CategoryHeaderLayout->addWidget(mCategoryTabs, 0);
	CategoryHeaderLayout->addStretch(1);
	mCategorySummaryLabel = new QLabel(CategoryHeader);
	mCategorySummaryLabel->setObjectName("CategorySummaryLabel");
	mCategorySummaryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	mCategorySummaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	mCategorySummaryLabel->setStyleSheet("color: #8a8a8a;");
	CategoryHeaderLayout->addWidget(mCategorySummaryLabel, 0);
	LeftLayout->addWidget(CategoryHeader);

	mFileListWidget = new QTreeWidget();
	mFileListWidget->setObjectName("AssetTree");
	mFileListWidget->setColumnCount(1);
	mFileListWidget->setHeaderHidden(true);
	mFileListWidget->setSelectionMode(QAbstractItemView::NoSelection);
	mFileListWidget->setUniformRowHeights(false);
	mFileListWidget->setRootIsDecorated(true);
	mFileListWidget->setAlternatingRowColors(false);
	mFileListWidget->setIndentation(18);
	mFileListWidget->header()->setStretchLastSection(false);
	mFileListWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	mFileListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	mFileListWidget->setContentsMargins(0, 0, 0, 0);
	mFileListWidget->viewport()->setObjectName("AssetTreeViewport");
	mFileListWidget->viewport()->setAttribute(Qt::WA_StyledBackground, true);
	mFileListWidget->viewport()->installEventFilter(this);
	mAssetTreeBackgroundOverlay = new QLabel(mFileListWidget->viewport());
	mAssetTreeBackgroundOverlay->setObjectName("AssetTreeBackgroundOverlay");
	mAssetTreeBackgroundOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	mAssetTreeBackgroundOverlay->setScaledContents(false);
	mAssetTreeBackgroundOverlay->setAlignment(Qt::AlignCenter);
	mAssetTreeBackgroundOverlay->hide();
	mAssetTreeBackgroundOverlay->lower();
	LeftLayout->addWidget(mFileListWidget);

	connect(mCategoryTabs, &QTabBar::currentChanged, this, [=](int Index)
	{
		if (Index < 0 || Index >= mCategoryTabs->count())
			return;
		if (IsAddCategoryTabKey(mCategoryTabs->tabData(Index).toString()))
		{
			const int LastRealTab = FindCategoryTabIndex(mCategoryTabs, QSettings().value("ActiveCategoryTabKey", "all").toString());
			const int RestoreIndex = (LastRealTab >= 0 && !IsAddCategoryTabKey(mCategoryTabs->tabData(LastRealTab).toString())) ? LastRealTab : 0;
			{
				QSignalBlocker Blocker(mCategoryTabs);
				mCategoryTabs->setCurrentIndex(RestoreIndex);
			}
			ShowCategoryTabEditDialog(-1);
			return;
		}
		QSettings().setValue("ActiveCategoryTab", Index);
		QSettings().setValue("ActiveCategoryTabKey", CurrentCategoryTabKey(mCategoryTabs));
		PopulateFileList();
	});

	connect(mCategoryTabs, &QTabBar::tabBarDoubleClicked, this, [=](int Index)
	{
		if (Index >= 0 && Index < mCategoryTabs->count() && !IsAddCategoryTabKey(mCategoryTabs->tabData(Index).toString()))
			ShowCategoryTabEditDialog(Index);
	});

	connect(mCategoryTabs, &QTabBar::customContextMenuRequested, this, [=](const QPoint& Position)
	{
		const int TabIndex = mCategoryTabs->tabAt(Position);
		if (TabIndex < 0 || TabIndex >= mCategoryTabs->count())
			return;

		const QString TabKey = mCategoryTabs->tabData(TabIndex).toString();
		if (IsAddCategoryTabKey(TabKey))
			return;

		QMenu Menu(mCategoryTabs);
		QAction* EditAction = Menu.addAction("Edit");
		const bool CustomTab = TabKey.startsWith("custom-");

		QWidgetAction* DeleteAction = new QWidgetAction(&Menu);
		QToolButton* DeleteButton = new QToolButton(&Menu);
		DeleteButton->setText("Delete");
		DeleteButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		DeleteButton->setAutoRaise(true);
		DeleteButton->setCursor(Qt::PointingHandCursor);
		DeleteButton->setEnabled(CustomTab);
		DeleteButton->setStyleSheet(
			"QToolButton { color:#d84a4a; font-weight:700; padding:4px 12px; text-align:left; }"
			"QToolButton:hover { background: rgba(216,74,74,0.16); }"
			"QToolButton:disabled { color:#7a7a7a; }");
		DeleteAction->setDefaultWidget(DeleteButton);
		Menu.addAction(DeleteAction);

		bool DeleteRequested = false;
		connect(DeleteButton, &QToolButton::clicked, &Menu, [&]()
		{
			DeleteRequested = true;
			Menu.close();
		});

		QAction* PickedAction = Menu.exec(mCategoryTabs->mapToGlobal(Position));
		if (!PickedAction && !DeleteRequested)
			return;

		if (PickedAction == EditAction)
		{
			ShowCategoryTabEditDialog(TabIndex);
			return;
		}

		if (DeleteRequested && CustomTab)
		{
			QString TabName = mCategoryTabs->tabText(TabIndex);
			if (TabName.contains("  "))
				TabName = TabName.mid(TabName.indexOf("  ") + 2).trimmed();

			const QMessageBox::StandardButton Reply = QMessageBox::question(
				this,
				"Delete Tab",
				QString("Delete the custom tab \"%1\"?\n\nThis does not delete any maps or mods.").arg(TabName),
				QMessageBox::Yes | QMessageBox::Cancel,
				QMessageBox::Cancel);

			if (Reply != QMessageBox::Yes)
				return;

			QList<QVariantMap> Defs = LoadCustomTabDefs();
			for (int i = 0; i < Defs.size(); ++i)
			{
				if (Defs[i].value("id").toString() == TabKey)
				{
					Defs.removeAt(i);
					break;
				}
			}
			SaveCustomTabDefs(Defs);
			RebuildCategoryTabs();
		}
	});

	connect(mFileListWidget, &QTreeWidget::customContextMenuRequested, this, [=](const QPoint& Position)
	{
		ContextMenuRequested(Position);
	});
	connect(mFileListWidget, &QTreeWidget::itemExpanded, this, [=](QTreeWidgetItem* Item)
	{
		if (!Item || Item->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
			return;
		QSettings().setValue(SectionSettingKey(Item->text(0)) + "/Expanded", true);
	});
	connect(mFileListWidget, &QTreeWidget::itemCollapsed, this, [=](QTreeWidgetItem* Item)
	{
		if (!Item || Item->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
			return;
		QSettings().setValue(SectionSettingKey(Item->text(0)) + "/Expanded", false);
	});
	connect(mFileListWidget, &QTreeWidget::currentItemChanged, this, [=](QTreeWidgetItem* CurrentItem, QTreeWidgetItem* PreviousItem)
		{
			SyncItemSelectionWidget(PreviousItem);
			SyncItemSelectionWidget(CurrentItem);
			SetCurrentStatsItem(CurrentItem);
			UpdateBuildActionButtons();
			UpdateQuickLaunchVisibility();
			UpdateGameRunningState();
		});

	mActionsPanel = new QWidget(mTopWidget);
	mActionsPanel->setObjectName("ActionsPanel");
	mActionsPanel->setMinimumWidth(220);
	mActionsPanel->setMaximumWidth(220);
	mActionsPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	QVBoxLayout* ActionsLayout = new QVBoxLayout(mActionsPanel);
	ActionsLayout->setContentsMargins(10, 8, 10, 8);
	ActionsLayout->setSpacing(6);

	QLabel* BuildOptionsLabel = new QLabel("Build Options");
	ActionsLayout->addWidget(BuildOptionsLabel);

	QHBoxLayout* CompileLayout = new QHBoxLayout();
	CompileLayout->setContentsMargins(0, 0, 0, 0);
	CompileLayout->setSpacing(4);
	ActionsLayout->addLayout(CompileLayout);

	mCompileEnabledWidget = new QCheckBox("Compile");
	CompileLayout->addWidget(mCompileEnabledWidget);

	mCompileModeWidget = new QComboBox();
	mCompileModeWidget->addItems(QStringList() << "Ents" << "Full");
	mCompileModeWidget->setCurrentIndex(1);
	CompileLayout->addWidget(mCompileModeWidget);

	QHBoxLayout* LightLayout = new QHBoxLayout();
	LightLayout->setContentsMargins(0, 0, 0, 0);
	LightLayout->setSpacing(4);
	ActionsLayout->addLayout(LightLayout);

	mLightEnabledWidget = new QCheckBox("Light");
	LightLayout->addWidget(mLightEnabledWidget);

	mLightQualityWidget = new QComboBox();
	mLightQualityWidget->addItems(QStringList() << "Low" << "Medium" << "High");
	mLightQualityWidget->setCurrentIndex(1);
	mLightQualityWidget->setMinimumWidth(64); // Fix for "Medium" being cut off in the dark theme
	LightLayout->addWidget(mLightQualityWidget);

	mLinkEnabledWidget = new QCheckBox("Link");
	ActionsLayout->addWidget(mLinkEnabledWidget);

	mRunEnabledWidget = new QCheckBox("Run");
	ActionsLayout->addWidget(mRunEnabledWidget);
	ActionsLayout->addSpacing(2);

	mRunOptionsWidget = new QLineEdit();
	mRunOptionsWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	mRunOptionsWidget->setPlaceholderText("Extra args...");
	ActionsLayout->addWidget(mRunOptionsWidget);
	connect(mCompileEnabledWidget, &QCheckBox::toggled, this, [=](bool) { UpdateBuildActionButtons(); });
	connect(mLightEnabledWidget, &QCheckBox::toggled, this, [=](bool) { UpdateBuildActionButtons(); });
	connect(mLinkEnabledWidget, &QCheckBox::toggled, this, [=](bool) { UpdateBuildActionButtons(); });
	connect(mRunEnabledWidget, &QCheckBox::toggled, this, [=](bool) { UpdateBuildActionButtons(); });
	mQuickLaunchLabel = new QLabel("Quick Launch Map");
	ActionsLayout->addWidget(mQuickLaunchLabel);

	mQuickLaunchWidget = new QuickLaunchPicker();
	AppendStartupTrace("ctor:quick-launch-widget-created");
	mQuickLaunchWidget->setMinimumWidth(0);
	mQuickLaunchWidget->SetControlHeights(34, 340);
	mQuickLaunchWidget->SetPopulateEntriesHandler([this]() { PopulateQuickLaunchEntries(); });
	mQuickLaunchWidget->SetPopulateWorkshopEntriesHandler([this]() { PopulateWorkshopQuickLaunchEntries(); });
	ActionsLayout->addWidget(mQuickLaunchWidget);
	mStartupQuoteLabel = new QLabel();
	mStartupQuoteLabel->setObjectName("StartupQuoteLabel");
	mStartupQuoteLabel->setWordWrap(true);
	mStartupQuoteLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	mStartupQuoteLabel->hide();
	ActionsLayout->addWidget(mStartupQuoteLabel);
	UpdateQuickLaunchVisibility();

	QLabel* BuildActionsLabel = new QLabel("Build Actions");
	ActionsLayout->addWidget(BuildActionsLabel);

	mBuildAllLanguagesButton = new QPushButton(QString("Build (%1)").arg(mBuildLanguage.compare("All", Qt::CaseInsensitive) == 0 ? QString("English") : mBuildLanguage));
	mBuildAllLanguagesButton->setObjectName("BuildEnglishButton");
	connect(mBuildAllLanguagesButton, SIGNAL(clicked()), mActionEditBuildAllLanguages, SLOT(trigger()));
	ActionsLayout->addWidget(mBuildAllLanguagesButton);

	mBuildButton = new QPushButton("Build (All)");
	mBuildButton->setObjectName("BuildButton");
	connect(mBuildButton, SIGNAL(clicked()), mActionEditBuild, SLOT(trigger()));
	ActionsLayout->addWidget(mBuildButton);
	UpdateBuildActionButtons();

	mDvarsButton = new QPushButton("Dvars");
	connect(mDvarsButton, SIGNAL(clicked()), this, SLOT(OnEditDvars()));
	ActionsLayout->addWidget(mDvarsButton);

	mLogButton = new QPushButton("Save Log");
	connect(mLogButton, SIGNAL(clicked()), this, SLOT(OnSaveLog()));
	ActionsLayout->addWidget(mLogButton);

	mIgnoreErrorsWidget = new QCheckBox("Ignore Errors");
	ActionsLayout->addWidget(mIgnoreErrorsWidget);
	const QString RunOnlineToolTip = "Launch through Steam online mode so custom maps and mods can appear online.";
	auto ShowRunOnlineGuide = [this, RunOnlineToolTip]()
	{
		QMessageBox::information(this, "Online Mode Guide",
			RunOnlineToolTip + "\n\n"
			"Use this when you want the game to launch through Steam instead of the direct executable. "
			"That helps custom maps and mods show up correctly in online mode. Workshop quick-launch maps use this route automatically.\n\n"
			"Leave it off if you want the normal direct launch behavior.");
	};
	mRunOnlineWidget = new QCheckBox("Online Mode");
	mRunOnlineWidget->setChecked(false);
	if (Settings.contains("RunOnlineMode"))
		mRunOnlineWidget->setChecked(Settings.value("RunOnlineMode").toBool());
	else
		Settings.setValue("RunOnlineMode", false);
	const bool ShowStartupQuote = Settings.contains("ShowStartupQuote") ? Settings.value("ShowStartupQuote").toBool() : false;
	if (!Settings.contains("ShowStartupQuote"))
		Settings.setValue("ShowStartupQuote", false);
	mStartupQuoteText = ShowStartupQuote ? RandomStartupQuote() : QString();
	if (mStartupQuoteLabel)
	{
		mStartupQuoteLabel->setText(mStartupQuoteText);
		mStartupQuoteLabel->setVisible(ShowStartupQuote);
	}
	QWidget* RunOnlineRow = new QWidget();
	QHBoxLayout* RunOnlineLayout = new QHBoxLayout(RunOnlineRow);
	RunOnlineLayout->setContentsMargins(0, 0, 0, 0);
	RunOnlineLayout->setSpacing(4);
	RunOnlineLayout->addWidget(mRunOnlineWidget);
	QToolButton* RunOnlineHelpButton = new QToolButton(RunOnlineRow);
	RunOnlineHelpButton->setText("?");
	RunOnlineHelpButton->setToolTip("Online mode guide");
	RunOnlineHelpButton->setAutoRaise(true);
	RunOnlineHelpButton->setCursor(Qt::WhatsThisCursor);
	RunOnlineHelpButton->setFixedSize(18, 18);
	connect(RunOnlineHelpButton, &QToolButton::clicked, this, ShowRunOnlineGuide);
	RunOnlineLayout->addWidget(RunOnlineHelpButton);
	RunOnlineLayout->addStretch(1);
	ActionsLayout->addWidget(RunOnlineRow);
	connect(mRunOnlineWidget, &QCheckBox::toggled, this, [=](bool Checked) { QSettings().setValue("RunOnlineMode", Checked); });
	mQuickLaunchWidget->SetSelectionChangedHandler([=](const QString& MapCode, const QString& WorkshopId)
	{
		if (mQuickLaunchWidget && mQuickLaunchWidget->IsWorkshopSelection() && mRunOnlineWidget && !mRunOnlineWidget->isChecked())
			mRunOnlineWidget->setChecked(true);

		QStringList ExistingArgs = mRunOptionsWidget->text().split(' ', Qt::SkipEmptyParts);
		for (int ArgIdx = ExistingArgs.count() - 1; ArgIdx >= 0; ArgIdx--)
		{
			if (ExistingArgs[ArgIdx] == "+devmap")
			{
				ExistingArgs.removeAt(ArgIdx);
				if (ArgIdx < ExistingArgs.count())
					ExistingArgs.removeAt(ArgIdx);
				continue;
			}
			if (ExistingArgs[ArgIdx] == "+set" && ArgIdx + 1 < ExistingArgs.count()
				&& (ExistingArgs[ArgIdx + 1] == "fs_game" || ExistingArgs[ArgIdx + 1] == "workshopid"))
			{
				const int RemoveCount = qMin(3, ExistingArgs.count() - ArgIdx);
				for (int RemoveIdx = 0; RemoveIdx < RemoveCount; RemoveIdx++)
					ExistingArgs.removeAt(ArgIdx);
				continue;
			}
			if (ExistingArgs[ArgIdx] == "+workshopid")
			{
				ExistingArgs.removeAt(ArgIdx);
				if (ArgIdx < ExistingArgs.count())
					ExistingArgs.removeAt(ArgIdx);
			}
		}

		if (MapCode.isEmpty())
		{
			mRunOptionsWidget->setText(ExistingArgs.join(' '));
			return;
		}

		if (mQuickLaunchWidget && mQuickLaunchWidget->IsWorkshopSelection())
		{
			ExistingArgs << "+set" << "fs_game" << "mod";
			if (!WorkshopId.trimmed().isEmpty())
				ExistingArgs << "+set" << "workshopid" << WorkshopId.trimmed();
		}
		else
		{
			ExistingArgs << "+devmap" << MapCode;
		}
		mRunOptionsWidget->setText(ExistingArgs.join(' '));
	});

	ActionsLayout->addSpacing(4);
	QLabel* GameControlLabel = new QLabel("Game Control");
	ActionsLayout->addWidget(GameControlLabel);
	mCloseGameButton = new QPushButton("Close Game");
	connect(mCloseGameButton, &QPushButton::clicked, this, [=]()
	{
		if (mGameRunningState != GameNotRunning)
		{
			mPendingLaunchRequestedMs = 0;
			QProcess::execute("taskkill", QStringList() << "/IM" << "BlackOps3.exe" << "/F");
			UpdateGameRunningState();
			return;
		}

		if (!mFileListWidget || !mFileListWidget->currentItem() || mFileListWidget->currentItem()->data(0, Qt::UserRole).toInt() == ML_ITEM_UNKNOWN)
			return;

		QTreeWidgetItem* Item = mFileListWidget->currentItem();
		const int ItemType = Item->data(0, Qt::UserRole).toInt();
		const QString MapName = (ItemType == ML_ITEM_MAP) ? GetItemEntryName(Item) : QString();
		const QString FsGame = (ItemType == ML_ITEM_MAP) ? GetItemEntryName(Item) : GetItemContainerName(Item);
		QueueGameStatsForItem(GetItemFavoriteKey(Item));
		TouchRecentEntry(RecentEntryForItem(ItemType, GetItemContainerName(Item), GetItemEntryName(Item)));
		const QPair<QString, QStringList> LaunchCommand = CreateGameLaunchCommand(FsGame, MapName);
		mPendingLaunchRequestedMs = mStatsElapsedTimer.isValid() ? mStatsElapsedTimer.elapsed() : 1;
		if (ShouldUseSteamOnlineLaunch())
			ShowOnlineLaunchProgressDialog(ItemType == ML_ITEM_MAP ? QString("map '%1'").arg(MapName) : QString("mod '%1'").arg(FsGame));
		QProcess::startDetached(LaunchCommand.first, LaunchCommand.second, QFileInfo(LaunchCommand.first).absolutePath());
		UpdateGameRunningState();
	});
	ActionsLayout->addWidget(mCloseGameButton);
	mCloseGameStatusLabel = new QLabel("Game is not running");
	mCloseGameStatusLabel->setObjectName("GameStateLabel");
	mCloseGameStatusLabel->setWordWrap(true);
	ActionsLayout->addWidget(mCloseGameStatusLabel);

	ActionsLayout->addStretch(1);

	mOutputWidget = new QTreeWidget(this);
	mOutputWidget->setObjectName("OutputConsole");
	mOutputWidget->setColumnCount(2);
	mOutputWidget->setHeaderHidden(true);
	mOutputWidget->setRootIsDecorated(false);
	mOutputWidget->setIndentation(0);
	mOutputWidget->setUniformRowHeights(false);
	mOutputWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
	mOutputWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	mOutputWidget->setFocusPolicy(Qt::StrongFocus);
	mOutputWidget->setAllColumnsShowFocus(false);
	mOutputWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	mOutputWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	mOutputWidget->header()->setStretchLastSection(false);
	mOutputWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	mOutputWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
	mOutputWidget->header()->resizeSection(1, 68);
	mOutputWidget->installEventFilter(this);
	mOutputWidget->viewport()->setObjectName("OutputConsoleViewport");
	mOutputWidget->viewport()->setAttribute(Qt::WA_StyledBackground, true);
	mOutputWidget->viewport()->installEventFilter(this);
	connect(mOutputWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, [=](int)
	{
		mOutputTreeAutoFollow = IsScrollBarAtBottom(mOutputWidget ? mOutputWidget->verticalScrollBar() : NULL);
	});
	mOutputPlainWidget = new QPlainTextEdit(this);
	mOutputPlainWidget->setObjectName("OutputConsolePlain");
	mOutputPlainWidget->setReadOnly(true);
	mOutputPlainWidget->setUndoRedoEnabled(false);
	mOutputPlainWidget->setLineWrapMode(QPlainTextEdit::NoWrap);
	mOutputPlainWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mOutputPlainWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mOutputPlainWidget->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	mOutputPlainWidget->setFocusPolicy(Qt::StrongFocus);
	mOutputPlainWidget->viewport()->setObjectName("OutputConsolePlainViewport");
	mOutputPlainWidget->viewport()->setAttribute(Qt::WA_StyledBackground, true);
	mOutputPlainWidget->viewport()->installEventFilter(this);
	connect(mOutputPlainWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, [=](int)
	{
		mOutputPlainAutoFollow = IsScrollBarAtBottom(mOutputPlainWidget ? mOutputPlainWidget->verticalScrollBar() : NULL);
	});
	mOutputPlainWidget->hide();
	mOutputBackgroundOverlay = new QLabel(mOutputWidget->viewport());
	mOutputBackgroundOverlay->setObjectName("OutputBackgroundOverlay");
	mOutputBackgroundOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	mOutputBackgroundOverlay->setScaledContents(false);
	mOutputBackgroundOverlay->setAlignment(Qt::AlignCenter);
	mOutputBackgroundOverlay->hide();
	mOutputBackgroundOverlay->lower();
	mOutputPanel = new QWidget(this);
	mOutputPanel->setObjectName("OutputPanel");
	QVBoxLayout* OutputPanelLayout = new QVBoxLayout(mOutputPanel);
	OutputPanelLayout->setContentsMargins(0, 0, 0, 0);
	OutputPanelLayout->setSpacing(0);
	mOutputTabs = new QTabBar(mOutputPanel);
	mOutputTabs->setObjectName("OutputTabs");
	mOutputTabs->setDocumentMode(true);
	mOutputTabs->setExpanding(false);
	mOutputTabs->setDrawBase(false);
	mOutputTabs->setMovable(true);
	mOutputTabs->setUsesScrollButtons(true);
	mOutputTabs->setElideMode(Qt::ElideRight);
	mOutputTabs->addTab("Full");
	mOutputTabs->addTab("Errors");
	mOutputTabs->addTab("Warnings");
	mOutputTabs->addTab("Materials");
	mOutputTabs->addTab("Images");
	mOutputTabs->addTab("xmodels");
	mOutputTabs->addTab("playercharacters");
	mOutputTabs->addTab("converting/mesh");
	mOutputTabs->addTab("techsets");
	mOutputTabs->addTab("shaders");
	mOutputTabs->setCurrentIndex(OutputLogTabFull);
	connect(mOutputTabs, &QTabBar::currentChanged, this, [=](int Index)
	{
		mOutputTabIndex = qMax(0, Index);
		RebuildOutputFromBuffer();
	});
	mLogFiltersButton = new QToolButton(this);
	mLogFiltersButton->setObjectName("LogFiltersButton");
	mLogFiltersButton->setText("Log Filters");
	mLogFiltersButton->setPopupMode(QToolButton::InstantPopup);
	QMenu* LogFiltersMenu = new QMenu(mLogFiltersButton);
	struct LogFilterMenuEntry
	{
		LogFilterKind Kind;
		const char* Label;
	};
	const LogFilterMenuEntry LogFilterEntries[] =
	{
		{ LogFilterLeakMod, "Show L3akMod" },
		{ LogFilterPhilLibX, "Show PhilLibX" },
		{ LogFilterGdtDb, "Show gdtDB" },
		{ LogFilterLinking, "Show Linking / done" },
		{ LogFilterCommands, "Show commands" },
		{ LogFilterWarnings, "Show warnings" },
		{ LogFilterErrors, "Show errors" }
	};
	for (int EntryIdx = 0; EntryIdx < static_cast<int>(sizeof(LogFilterEntries) / sizeof(LogFilterEntries[0])); EntryIdx++)
	{
		const LogFilterMenuEntry Entry = LogFilterEntries[EntryIdx];
		QAction* FilterAction = LogFiltersMenu->addAction(Entry.Label);
		FilterAction->setCheckable(true);
		FilterAction->setChecked(IsLogFilterEnabled(Settings, Entry.Kind));
		connect(FilterAction, &QAction::toggled, this, [Entry](bool Enabled)
		{
			QSettings FilterSettings;
			FilterSettings.setValue(LogFilterSettingKey(Entry.Kind), Enabled);
		});
	}
	mLogFiltersButton->setMenu(LogFiltersMenu);
	OutputPanelLayout->addWidget(mOutputTabs, 0);
	OutputPanelLayout->addWidget(mOutputWidget, 1);
	OutputPanelLayout->addWidget(mOutputPlainWidget, 1);
	mAssetDockWidget = new QDockWidget("Map/Mods List", this);
	mAssetDockWidget->setObjectName("AssetDockWidget");
	mAssetDockWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
	mAssetDockWidget->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	mAssetDockWidget->setWidget(mLeftPanel);
	mOutputDockWidget = new QDockWidget("Console", this);
	mOutputDockWidget->setObjectName("OutputDockWidget");
	mOutputDockWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
	mOutputDockWidget->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	mOutputDockWidget->setWidget(mOutputPanel);
	setDockNestingEnabled(true);
	setDockOptions(dockOptions() | QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
	ApplyLauncherLayout();
	AppendStartupTrace("ctor:layout-applied");

	setCentralWidget(mTopWidget);
	AppendStartupTrace("ctor:central-widget-set");
	mFooterRefreshButton = new QToolButton(this);
	mFooterRefreshButton->setObjectName("FooterRefreshButton");
	mFooterRefreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	mFooterRefreshButton->setToolTip("Check for launcher updates");
	mFooterRefreshButton->setAutoRaise(true);
	connect(mFooterRefreshButton, &QToolButton::clicked, this, &mlMainWindow::OnCheckForUpdates);
	mFooterUpdateStatusLabel = new QLabel(QString("v%1 - Up to date").arg(kLauncherVersion), this);
	mFooterUpdateStatusLabel->setObjectName("FooterStatusLabel");
	mFooterDownloadButton = new QPushButton("Download", this);
	mFooterDownloadButton->setObjectName("FooterDownloadButton");
	mFooterDownloadButton->setVisible(false);
	connect(mFooterDownloadButton, &QPushButton::clicked, this, [this]()
	{
		if (mAvailableUpdateUrl.trimmed().isEmpty())
		{
			if (!mAvailableReleasePageUrl.trimmed().isEmpty())
				QDesktopServices::openUrl(QUrl(mAvailableReleasePageUrl));
			return;
		}

		SetFooterUpdateState(QString("Downloading %1...").arg(mAvailableUpdateVersion), mAvailableUpdateUrl, mAvailableUpdateVersion, mAvailableReleasePageUrl, true);
		StartUpdateDownload(QUrl(mAvailableUpdateUrl), mAvailableUpdateVersion);
	});
	QLabel* FooterCreditsLabel = new QLabel("Edits by Sphynx", this);
	FooterCreditsLabel->setObjectName("FooterStatusLabel");
	const QString FooterDropdownStyle =
		"QToolButton { background: transparent; border: 0; padding: 0 14px 0 8px; min-height: 18px; }"
		"QToolButton:hover { background: transparent; border: 0; }"
		"QToolButton:pressed { background: transparent; border: 0; }"
		"QToolButton::menu-button { border: 0; width: 12px; }"
		"QToolButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 2px; }";
	QToolButton* WindowFiltersButton = new QToolButton(this);
	WindowFiltersButton->setObjectName("WindowFiltersButton");
	WindowFiltersButton->setText("Windows");
	WindowFiltersButton->setPopupMode(QToolButton::InstantPopup);
	WindowFiltersButton->setStyleSheet(FooterDropdownStyle);
	QMenu* WindowFiltersMenu = new QMenu(WindowFiltersButton);
	QAction* ToggleAssetTreeAction = WindowFiltersMenu->addAction("Map/Mods List");
	ToggleAssetTreeAction->setCheckable(true);
	ToggleAssetTreeAction->setChecked(mAssetDockWidget && mAssetDockWidget->isVisible());
	QAction* ToggleConsoleAction = WindowFiltersMenu->addAction("Console / Log");
	ToggleConsoleAction->setCheckable(true);
	ToggleConsoleAction->setChecked(mOutputDockWidget && mOutputDockWidget->isVisible());
	connect(ToggleAssetTreeAction, &QAction::toggled, this, [=](bool Visible)
	{
		if (!mAssetDockWidget)
			return;
		mAssetDockWidget->setVisible(Visible);
		if (Visible)
			mAssetDockWidget->raise();
	});
	connect(ToggleConsoleAction, &QAction::toggled, this, [=](bool Visible)
	{
		if (!mOutputDockWidget)
			return;
		mOutputDockWidget->setVisible(Visible);
		if (Visible)
			mOutputDockWidget->raise();
	});
	connect(mAssetDockWidget, &QDockWidget::visibilityChanged, this, [=](bool Visible)
	{
		QSignalBlocker Blocker(ToggleAssetTreeAction);
		ToggleAssetTreeAction->setChecked(Visible);
	});
	connect(mOutputDockWidget, &QDockWidget::visibilityChanged, this, [=](bool Visible)
	{
		QSignalBlocker Blocker(ToggleConsoleAction);
		ToggleConsoleAction->setChecked(Visible);
	});
	WindowFiltersButton->setMenu(WindowFiltersMenu);
	QToolButton* ConsoleDisplayButton = new QToolButton(this);
	ConsoleDisplayButton->setObjectName("ConsoleDisplayButton");
	ConsoleDisplayButton->setText("Console Display");
	ConsoleDisplayButton->setPopupMode(QToolButton::InstantPopup);
	ConsoleDisplayButton->setStyleSheet(FooterDropdownStyle);
	QMenu* ConsoleDisplayMenu = new QMenu(ConsoleDisplayButton);
	QActionGroup* ConsoleColorCodingGroup = new QActionGroup(ConsoleDisplayMenu);
	ConsoleColorCodingGroup->setExclusive(true);
	QAction* ColorCodingEnabledAction = ConsoleDisplayMenu->addAction("Color Coding: Enabled");
	ColorCodingEnabledAction->setCheckable(true);
	ColorCodingEnabledAction->setActionGroup(ConsoleColorCodingGroup);
	QAction* ColorCodingDisabledAction = ConsoleDisplayMenu->addAction("Color Coding: Disabled");
	ColorCodingDisabledAction->setCheckable(true);
	ColorCodingDisabledAction->setActionGroup(ConsoleColorCodingGroup);
	const bool ConsoleColorCodingEnabled = Settings.value("ConsoleColorCodingEnabled", true).toBool();
	ColorCodingEnabledAction->setChecked(ConsoleColorCodingEnabled);
	ColorCodingDisabledAction->setChecked(!ConsoleColorCodingEnabled);
	connect(ColorCodingEnabledAction, &QAction::triggered, this, [=]()
	{
		QSettings().setValue("ConsoleColorCodingEnabled", true);
		UpdateOutputConsoleMode();
		RebuildOutputFromBuffer();
	});
	connect(ColorCodingDisabledAction, &QAction::triggered, this, [=]()
	{
		QSettings().setValue("ConsoleColorCodingEnabled", false);
		UpdateOutputConsoleMode();
		RebuildOutputFromBuffer();
	});
	ConsoleDisplayButton->setMenu(ConsoleDisplayMenu);
	mLogFiltersButton->setStyleSheet(FooterDropdownStyle);
	statusBar()->addWidget(mFooterRefreshButton, 0);
	statusBar()->addWidget(mFooterUpdateStatusLabel, 0);
	statusBar()->addWidget(mFooterDownloadButton, 0);
	statusBar()->addPermanentWidget(WindowFiltersButton, 0);
	statusBar()->addPermanentWidget(ConsoleDisplayButton, 0);
	statusBar()->addPermanentWidget(mLogFiltersButton, 0);
	statusBar()->addPermanentWidget(FooterCreditsLabel, 0);
	SetFooterUpdateState("Up to date");

	mShippedMapList << "mp_aerospace" <<  "mp_apartments" << "mp_arena" << "mp_banzai" << "mp_biodome" << "mp_chinatown" << "mp_city" << "mp_conduit" << "mp_crucible" << "mp_cryogen" << "mp_ethiopia" << "mp_freerun_01" << "mp_freerun_02" << "mp_freerun_03" << "mp_freerun_04" << "mp_havoc" << "mp_infection" << "mp_kung_fu" << "mp_metro" << "mp_miniature" << "mp_nuketown_x" << "mp_redwood" << "mp_rise" << "mp_rome" << "mp_ruins" << "mp_sector" << "mp_shrine" << "mp_skyjacked" << "mp_spire" << "mp_stronghold" << "mp_veiled" << "mp_waterpark" << "mp_western" << "zm_castle" << "zm_factory" << "zm_genesis" << "zm_island" << "zm_levelcommon" << "zm_stalingrad" << "zm_zod";

	Settings.beginGroup("MainWindow");
	resize(QSize(1280, 1020));
	move(QPoint(200, 200));
	restoreGeometry(Settings.value("Geometry").toByteArray());
	restoreState(Settings.value("State").toByteArray());
	Settings.endGroup();
	AppendStartupTrace("ctor:window-state-restored");

	SteamAPI_Init();
	AppendStartupTrace("ctor:steam-init-called");

	connect(&mTimer, SIGNAL(timeout()), this, SLOT(SteamUpdate()));
	mTimer.start(1500);
	UpdateGameRunningState();
	AppendStartupTrace("ctor:steam-timer-ready");

	UpdateTheme();
	AppendStartupTrace("ctor:update-theme-done");
	PopulateFileList();
	AppendStartupTrace("ctor:populate-file-list-done");
	QTimer::singleShot(250, this, [this]()
	{
		AppendStartupTrace("ctor:delayed-populate-start");
		PopulateFileList();
		AppendStartupTrace("ctor:delayed-populate-done");
	});
	const bool AutoCheckOnStartup = Settings.value("UpdateCheck/AutoOnStartup", true).toBool();
	if (AutoCheckOnStartup)
		QTimer::singleShot(2000, this, [this]() { CheckForUpdates(false); });
	qApp->installEventFilter(this);
	AppendStartupTrace("ctor:done");
}

mlMainWindow::~mlMainWindow()
{
	if (mUpdateDownloadReply)
		mUpdateDownloadReply->abort();
	if (mUpdateDownloadFile)
	{
		if (mUpdateDownloadFile->isOpen())
			mUpdateDownloadFile->close();
		delete mUpdateDownloadFile;
		mUpdateDownloadFile = NULL;
	}
}

void mlMainWindow::showEvent(QShowEvent* Event)
{
	QMainWindow::showEvent(Event);

	if (mStartupQuotePopupShown)
		return;

	mStartupQuotePopupShown = true;
	if (mStartupQuoteText.isEmpty())
		return;

	QTimer::singleShot(0, this, [this]()
	{
		if (!isVisible() || mStartupQuoteText.isEmpty())
			return;
		QMessageBox::information(this, "Inspirational Quote", mStartupQuoteText);
	});
}

void mlMainWindow::EnsureThemeProfiles()
{
	QSettings Settings;
	const QStringList BuiltInIds = QStringList() << "original-updated" << "original-classic" << "dark-modern";
	for (const QString& ThemeProfileId : BuiltInIds)
	{
		Settings.beginGroup(kThemeProfilesGroup);
		Settings.beginGroup(ThemeProfileId);
		if (!Settings.contains("Name"))
			Settings.setValue("Name", ThemeProfileDisplayNameForBuiltInId(ThemeProfileId));
		const QVariantMap DefaultValues = DefaultThemeProfileValues(ThemeProfileId);
		for (auto It = DefaultValues.constBegin(); It != DefaultValues.constEnd(); ++It)
		{
			if (!Settings.contains(It.key()))
				Settings.setValue(It.key(), It.value());
		}
		Settings.endGroup();
		Settings.endGroup();
	}

	if (!Settings.contains(kThemeProfileSettingKey))
		Settings.setValue(kThemeProfileSettingKey, BuiltInThemeProfileId(ThemeModeFromSettings(Settings)));
}

QString mlMainWindow::CurrentThemeProfileId() const
{
	QSettings Settings;
	QString ThemeProfileId = Settings.value(kThemeProfileSettingKey, BuiltInThemeProfileId(mThemeMode)).toString().trimmed().toLower();
	return ThemeProfileId.isEmpty() ? QString("original-classic") : ThemeProfileId;
}

QStringList mlMainWindow::AvailableThemeProfileIds() const
{
	QSettings Settings;
	QStringList ThemeProfileIds = QStringList() << "original-updated" << "original-classic" << "dark-modern";
	Settings.beginGroup(kThemeProfilesGroup);
	const QStringList SavedIds = Settings.childGroups();
	Settings.endGroup();
	for (const QString& SavedId : SavedIds)
	{
		if (!ThemeProfileIds.contains(SavedId))
			ThemeProfileIds << SavedId;
	}
	return ThemeProfileIds;
}

QString mlMainWindow::ThemeProfileDisplayName(const QString& ThemeProfileId) const
{
	QSettings Settings;
	Settings.beginGroup(kThemeProfilesGroup);
	Settings.beginGroup(ThemeProfileId);
	const QString Name = Settings.value("Name", ThemeProfileDisplayNameForBuiltInId(ThemeProfileId)).toString().trimmed();
	Settings.endGroup();
	Settings.endGroup();
	return Name.isEmpty() ? ThemeProfileDisplayNameForBuiltInId(ThemeProfileId) : Name;
}

QVariantMap mlMainWindow::ThemeProfileValues(const QString& ThemeProfileId) const
{
	QVariantMap Values = DefaultThemeProfileValues(ThemeProfileId);
	QSettings Settings;
	Settings.beginGroup(kThemeProfilesGroup);
	Settings.beginGroup(ThemeProfileId);
	for (const QString& Key : ThemeProfileSettingKeys())
	{
		if (Settings.contains(Key))
			Values.insert(Key, Settings.value(Key));
	}
	Settings.endGroup();
	Settings.endGroup();
	return Values;
}

void mlMainWindow::SaveThemeProfile(const QString& ThemeProfileId, const QString& DisplayName, const QVariantMap& Values)
{
	QSettings Settings;
	Settings.beginGroup(kThemeProfilesGroup);
	Settings.beginGroup(ThemeProfileId);
	Settings.setValue("Name", DisplayName.trimmed().isEmpty() ? ThemeProfileDisplayNameForBuiltInId(ThemeProfileId) : DisplayName.trimmed());
	for (const QString& Key : ThemeProfileSettingKeys())
	{
		if (Values.contains(Key))
			Settings.setValue(Key, Values.value(Key));
	}
	Settings.endGroup();
	Settings.endGroup();
}

void mlMainWindow::ApplyThemeProfile(const QString& ThemeProfileId)
{
	const QVariantMap Values = ThemeProfileValues(ThemeProfileId);
	QSettings Settings;
	for (const QString& Key : ThemeProfileSettingKeys())
	{
		if (Values.contains(Key))
			Settings.setValue(Key, Values.value(Key));
	}
	Settings.setValue(kThemeProfileSettingKey, ThemeProfileId);
	mThemeProfileId = ThemeProfileId;
	mThemeMode = ThemeModeFromSettings(Settings);
	mLauncherLayout = Settings.value("LauncherLayout", mLauncherLayout).toString().trimmed().toLower();
}

void mlMainWindow::ShowOnlineLaunchProgressDialog(const QString& TargetLabel)
{
	mPendingOnlineLaunchFeedback = true;
	mPendingOnlineLaunchLabel = TargetLabel.trimmed().isEmpty() ? QString("selected content") : TargetLabel.trimmed();

	if (!mLaunchProgressDialog)
	{
		mLaunchProgressDialog = new QProgressDialog(this);
		mLaunchProgressDialog->setWindowTitle("Starting Game");
		mLaunchProgressDialog->setMinimumDuration(0);
		mLaunchProgressDialog->setRange(0, 0);
		mLaunchProgressDialog->setAutoClose(false);
		mLaunchProgressDialog->setAutoReset(false);
		mLaunchProgressDialog->setCancelButtonText("Hide");
		mLaunchProgressDialog->setWindowModality(Qt::WindowModal);
		mLaunchProgressDialog->resize(720, 160);
		if (ThemeUsesClassicChrome(mThemeMode))
		{
			mLaunchProgressDialog->setStyleSheet(
				"QProgressDialog { background: #5a5a5a; color: #111111; }"
				"QLabel { color: #111111; font-weight: 600; }"
				"QProgressBar { background: #4a4a4a; border: 1px solid #3a3a3a; border-radius: 10px; min-height: 22px; color: #111111; }"
				"QProgressBar::chunk { background: #ff8a2a; border-radius: 8px; margin: 2px; }"
				"QPushButton { background: #4a4a4a; border: 1px solid #3a3a3a; border-radius: 8px; padding: 8px 14px; color: #111111; }");
		}
		else if (ThemeUsesDarkModernChrome(mThemeMode))
		{
			mLaunchProgressDialog->setStyleSheet(
				"QProgressDialog { background: #111418; color: #eef1f4; }"
				"QLabel { color: #eef1f4; font-weight: 600; }"
				"QProgressBar { background: #1a2026; border: 1px solid #2d353d; border-radius: 10px; min-height: 22px; }"
				"QProgressBar::chunk { background: #ff8a2a; border-radius: 8px; margin: 2px; }"
				"QPushButton { background: #242424; border: 1px solid #3a3a3a; border-radius: 8px; padding: 8px 14px; color: #eef1f4; }");
		}
		connect(mLaunchProgressDialog, &QProgressDialog::canceled, this, [=]()
		{
			if (mLaunchProgressDialog)
				mLaunchProgressDialog->hide();
		});
	}

	mLaunchProgressDialog->setLabelText(
		QString("Starting %1 through Steam online mode...\n\nThis can take a bit while Steam hands off to Black Ops 3. The launcher will update automatically once the game begins opening.")
			.arg(mPendingOnlineLaunchLabel));
	mLaunchProgressDialog->show();
}

void mlMainWindow::CloseOnlineLaunchProgressDialog()
{
	mPendingOnlineLaunchFeedback = false;
	mPendingOnlineLaunchLabel.clear();
	if (mLaunchProgressDialog)
		mLaunchProgressDialog->hide();
}

void mlMainWindow::CreateActions()
{
	mActionFileNew = new QAction(ToolbarIconForActionKey("file-new"), "&New...", this);
	mActionFileNew->setObjectName("file-new");
	mActionFileNew->setShortcut(QKeySequence("Ctrl+N"));
	connect(mActionFileNew, SIGNAL(triggered()), this, SLOT(OnFileNew()));

	mActionFileAssetEditor = new QAction(ToolbarIconForActionKey("file-asset-editor"), "Asset Editor (APE)", this);
	mActionFileAssetEditor->setObjectName("file-asset-editor");
	mActionFileAssetEditor->setShortcut(QKeySequence("Ctrl+A"));
	connect(mActionFileAssetEditor, SIGNAL(triggered()), this, SLOT(OnFileAssetEditor()));

	mActionFileOpenRoot = new QAction(QIcon(QCoreApplication::applicationDirPath() + "/folderIcon.png"), "Open", this);
	mActionFileOpenRoot->setObjectName("file-open-root");
	mActionFileOpenRoot->setToolTip("Open the Black Ops 3 root folder");
	connect(mActionFileOpenRoot, SIGNAL(triggered()), this, SLOT(OnOpenModRootFolder()));

	mActionFileOpenConsoleLog = new QAction(style()->standardIcon(QStyle::SP_FileIcon), "Open console_mp.log", this);
	mActionFileOpenConsoleLog->setObjectName("file-open-console-log");
	mActionFileOpenConsoleLog->setToolTip("Open console_mp.log in the launcher");
	connect(mActionFileOpenConsoleLog, SIGNAL(triggered()), this, SLOT(OnOpenConsoleLog()));

	mActionFileOpenConsoleLogExternal = new QAction(style()->standardIcon(QStyle::SP_FileDialogListView), "Open console_mp.log in Editor", this);
	mActionFileOpenConsoleLogExternal->setObjectName("file-open-console-log-editor");
	mActionFileOpenConsoleLogExternal->setToolTip("Open console_mp.log in the default text editor");
	connect(mActionFileOpenConsoleLogExternal, SIGNAL(triggered()), this, SLOT(OnOpenConsoleLogExternal()));

	mActionFileOpenScriptReference = new QAction(ToolbarIconForActionKey("help-script-reference"), "BO3 Script Reference", this);
	mActionFileOpenScriptReference->setObjectName("help-script-reference");
	mActionFileOpenScriptReference->setToolTip("Open the BO3 Script Reference website");
	connect(mActionFileOpenScriptReference, SIGNAL(triggered()), this, SLOT(OnOpenScriptReference()));

	mActionFileLevelEditor = new QAction(ToolbarIconForActionKey("file-level-editor"), "Open Radiant", this);
	mActionFileLevelEditor->setObjectName("file-level-editor");
	mActionFileLevelEditor->setShortcut(QKeySequence("Ctrl+R"));
	mActionFileLevelEditor->setToolTip("Level Editor");
	connect(mActionFileLevelEditor, SIGNAL(triggered()), this, SLOT(OnFileLevelEditor()));

	mActionFileExport2Bin = new QAction(ToolbarIconForActionKey("file-export2bin"), "Export2Bin GUI", this);
	mActionFileExport2Bin->setObjectName("file-export2bin");
	mActionFileExport2Bin->setShortcut(QKeySequence("Ctrl+E"));
	connect(mActionFileExport2Bin, SIGNAL(triggered()), this, SLOT(OnFileExport2Bin()));

	mActionFileExit = new QAction("E&xit", this);
	connect(mActionFileExit, SIGNAL(triggered()), this, SLOT(close()));

	mActionEditBuild = new QAction(ToolbarIconForActionKey("edit-build"), "Build", this);
	mActionEditBuild->setObjectName("edit-build");
	mActionEditBuild->setShortcut(QKeySequence("Ctrl+B"));
	connect(mActionEditBuild, SIGNAL(triggered()), this, SLOT(OnEditBuild()));

	mActionEditBuildAllLanguages = new QAction("Build (English)", this);
	mActionEditBuildAllLanguages->setShortcut(QKeySequence("Ctrl+Shift+B"));
	connect(mActionEditBuildAllLanguages, SIGNAL(triggered()), this, SLOT(OnEditBuildAllLanguages()));

	mActionEditAnalyze = new QAction(ToolbarIconForActionKey("edit-analyze"), "Analyze", this);
	mActionEditAnalyze->setObjectName("edit-analyze");
	connect(mActionEditAnalyze, SIGNAL(triggered()), this, SLOT(OnAnalyzeItem()));

	mActionEditInformation = new QAction(style()->standardIcon(QStyle::SP_FileDialogInfoView), "Information", this);
	mActionEditInformation->setObjectName("edit-information");
	connect(mActionEditInformation, &QAction::triggered, this, [this]()
	{
		ShowItemInformationDialog(ActiveFileItem());
	});

	mActionEditReadyForPublish = new QAction(ToolbarIconForActionKey("edit-ready-for-publish"), "Publish Check", this);
	mActionEditReadyForPublish->setObjectName("edit-ready-for-publish");
	connect(mActionEditReadyForPublish, SIGNAL(triggered()), this, SLOT(OnEditReadyForPublish()));

	mActionEditPublish = new QAction(ToolbarIconForActionKey("edit-publish"), "Publish", this);
	mActionEditPublish->setObjectName("edit-publish");
	mActionEditPublish->setShortcut(QKeySequence("Ctrl+P"));
	connect(mActionEditPublish, SIGNAL(triggered()), this, SLOT(OnEditPublish()));

	mActionEditOptions = new QAction("&Options...", this);
	connect(mActionEditOptions, SIGNAL(triggered()), this, SLOT(OnEditOptions()));

	mActionHelpCheckUpdates = new QAction("Check for &Updates...", this);
	connect(mActionHelpCheckUpdates, SIGNAL(triggered()), this, SLOT(OnCheckForUpdates()));
	
	mActionHelpCredits = new QAction("&Credits...", this);
	connect(mActionHelpCredits, SIGNAL(triggered()), this, SLOT(OnHelpCredits()));
	
	mActionHelpAbout = new QAction("&About...", this);
	connect(mActionHelpAbout, SIGNAL(triggered()), this, SLOT(OnHelpAbout()));

	mActionHelpGuide = new QAction("&Guide / What's New...", this);
	connect(mActionHelpGuide, SIGNAL(triggered()), this, SLOT(OnHelpGuide()));
}

void mlMainWindow::CreateMenu()
{
	QMenuBar* MenuBar = new QMenuBar(this);
	auto BuildOpenMenu = [this](QWidget* Parent) -> QMenu*
	{
		QMenu* OpenMenu = new QMenu("Open", Parent);
		struct OpenFolderEntry
		{
			const char* Label;
			const char* RelativePath;
		};
		const OpenFolderEntry Entries[] =
		{
			{ "Root", "" },
			{ "Mods", "mods" },
			{ "Usermaps", "usermaps" },
			{ "source_data", "source_data" },
			{ "raw", "raw" },
			{ "texture_assets", "texture_assets" },
			{ "model_export", "model_export" }
		};
		for (int EntryIdx = 0; EntryIdx < static_cast<int>(sizeof(Entries) / sizeof(Entries[0])); EntryIdx++)
		{
			const QString FolderPath = Entries[EntryIdx].RelativePath[0] == '\0'
				? QDir::cleanPath(mGamePath)
				: QDir::cleanPath(QString("%1/%2").arg(mGamePath, Entries[EntryIdx].RelativePath));
			if (!QFileInfo(FolderPath).exists())
				continue;

			QAction* OpenAction = OpenMenu->addAction(Entries[EntryIdx].Label);
			connect(OpenAction, &QAction::triggered, this, [FolderPath]()
			{
				ShellExecute(NULL, "open", QString("\"%1\"").arg(FolderPath).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
			});
		}
		return OpenMenu;
	};
	
	QMenu* FileMenu = new QMenu("&File", MenuBar);
	FileMenu->addAction(mActionFileNew);
	FileMenu->addSeparator();
	FileMenu->addMenu(BuildOpenMenu(FileMenu));
	FileMenu->addAction(mActionFileOpenConsoleLog);
	FileMenu->addAction(mActionFileOpenConsoleLogExternal);
	FileMenu->addAction(mActionFileAssetEditor);
	FileMenu->addAction(mActionFileLevelEditor);
	FileMenu->addAction(mActionFileExport2Bin);
	FileMenu->addSeparator();
	FileMenu->addAction(mActionFileExit);
	MenuBar->addAction(FileMenu->menuAction());

	QMenu* EditMenu = new QMenu("&Edit", MenuBar);
	EditMenu->addAction(mActionEditBuild);
	EditMenu->addAction(mActionEditBuildAllLanguages);
	EditMenu->addAction(mActionEditAnalyze);
	EditMenu->addAction(mActionEditInformation);
	EditMenu->addAction(mActionEditReadyForPublish);
	EditMenu->addAction(mActionEditPublish);
	MenuBar->addAction(EditMenu->menuAction());

	QMenu* ThemesMenu = new QMenu("&Themes", MenuBar);
	QActionGroup* ThemeGroup = new QActionGroup(ThemesMenu);
	ThemeGroup->setExclusive(true);
		mActionThemeModern = ThemesMenu->addAction("Original Updated", this, SLOT(OnSetModernTheme()));
		mActionThemeClassic = ThemesMenu->addAction("Original Classic", this, SLOT(OnSetClassicTheme()));
		mActionThemeDarkModern = ThemesMenu->addAction("Dark Modern", this, SLOT(OnSetDarkModernTheme()));
	mActionThemeModern->setCheckable(true);
	mActionThemeDarkModern->setCheckable(true);
	mActionThemeClassic->setCheckable(true);
		ThemeGroup->addAction(mActionThemeModern);
		ThemeGroup->addAction(mActionThemeClassic);
		ThemeGroup->addAction(mActionThemeDarkModern);
	MenuBar->addAction(ThemesMenu->menuAction());

	QMenu* OptionsMenu = new QMenu("&Options", MenuBar);
	OptionsMenu->addAction(mActionEditOptions);
	MenuBar->addAction(OptionsMenu->menuAction());

	QMenu* HelpMenu = new QMenu("&Help", MenuBar);
	HelpMenu->addAction(mActionHelpGuide);
	HelpMenu->addSeparator();
	HelpMenu->addAction(mActionHelpCheckUpdates);
	HelpMenu->addAction(mActionFileOpenScriptReference);
	HelpMenu->addSeparator();
	HelpMenu->addAction(mActionHelpCredits);
	HelpMenu->addAction(mActionHelpAbout);
	MenuBar->addAction(HelpMenu->menuAction());

	setMenuBar(MenuBar);
	UpdateThemeMenuChecks();
}

void mlMainWindow::CreateToolBar()
{
	if (mMainToolBar)
	{
		removeToolBar(mMainToolBar);
		mMainToolBar->deleteLater();
		mMainToolBar = NULL;
	}

	QToolBar* ToolBar = new QToolBar("Standard", this);
	ToolBar->setObjectName(QStringLiteral("StandardToolBar"));
	ToolBar->setMovable(false);
	ToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ToolBar->setIconSize(QSize(18, 18));
	ToolBar->setContentsMargins(0, 1, 0, 0);

	const QList<ToolbarItemConfig> ToolbarItems = LoadToolbarItems();
	bool HasAnyVisibleItems = false;
	bool AddedExtraToolsButton = false;
	auto AddExtraToolsButton = [&]()
	{
		QToolButton* ExtraToolsButton = new QToolButton(ToolBar);
		ExtraToolsButton->setObjectName("ExtraToolsDropDownButton");
		ExtraToolsButton->setIcon(ToolbarIconForActionKey("extra-tools-menu"));
		ExtraToolsButton->setText("Extra Tools");
		ExtraToolsButton->setPopupMode(QToolButton::InstantPopup);
		ExtraToolsButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		ExtraToolsButton->setStyleSheet(
			"QToolButton { margin: 0; padding: 6px 8px 5px 8px; border: 1px solid transparent; }"
			"QToolButton:hover { margin: 0; padding: 6px 8px 5px 8px; background: rgba(255, 255, 255, 0.10); border: 1px solid #6a6a6a; }"
			"QToolButton:pressed { margin: 0; padding: 6px 8px 5px 8px; background: rgba(255, 255, 255, 0.16); border: 1px solid #7a7a7a; }"
			"QToolButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: top right; right: 4px; top: 4px; width: 10px; height: 10px; image: url(:/stylesheet/stylesheet/down_arrow.png); }"
			"QToolButton::menu-button { width: 14px; border: 0; }");
		QMenu* ExtraToolsMenu = new QMenu(ExtraToolsButton);
		ExtraToolsMenu->setStyleSheet(
			"QMenu::item { padding: 10px 18px 10px 40px; }"
			"QMenu::icon { left: 12px; }"
			"QMenu::right-arrow { width: 12px; height: 12px; margin-right: 6px; }");
		ExtraToolsMenu->addAction(mActionFileExport2Bin);
		QMenu* OpenLogsMenu = ExtraToolsMenu->addMenu(style()->standardIcon(QStyle::SP_FileDialogDetailedView), "Open Logs");
		OpenLogsMenu->setStyleSheet(ExtraToolsMenu->styleSheet());
		OpenLogsMenu->addAction(mActionFileOpenConsoleLog);
		OpenLogsMenu->addAction(mActionFileOpenConsoleLogExternal);
		ExtraToolsMenu->addAction(mActionFileOpenScriptReference);
		ExtraToolsButton->setMenu(ExtraToolsMenu);
		ToolBar->addWidget(ExtraToolsButton);
		HasAnyVisibleItems = true;
		AddedExtraToolsButton = true;
	};
	auto ShouldInsertGroupSeparatorBefore = [&](const QString& BuiltInKey) -> bool
	{
		return BuiltInKey == "edit-ready-for-publish"
			|| BuiltInKey == "file-asset-editor"
			|| BuiltInKey == "extra-tools-menu";
	};
	for (int ItemIdx = 0; ItemIdx < ToolbarItems.count(); ItemIdx++)
	{
		const ToolbarItemConfig Item = ToolbarItems[ItemIdx];
		if (Item.Hidden)
			continue;
		if (Item.BuiltIn && (Item.BuiltInActionKey == "file-open-console-log-editor"
			|| Item.BuiltInActionKey == "file-open-console-log"
			|| Item.BuiltInActionKey == "file-export2bin"
			|| Item.BuiltInActionKey == "help-script-reference"))
			continue;

		if (Item.BuiltIn && ShouldInsertGroupSeparatorBefore(Item.BuiltInActionKey) && HasAnyVisibleItems)
			ToolBar->addSeparator();

		if (Item.BuiltIn)
		{
			if (Item.BuiltInActionKey == "extra-tools-menu")
			{
				AddExtraToolsButton();
				continue;
			}

			QAction* Action = FindBuiltinToolbarAction(this, Item.BuiltInActionKey);
			if (!Action)
				continue;
			if (!Item.IconPath.trimmed().isEmpty() && QFileInfo(Item.IconPath).exists())
				Action->setIcon(QIcon(Item.IconPath));
			ToolBar->addAction(Action);
			HasAnyVisibleItems = true;
			continue;
		}

		QIcon ItemIcon;
		if (!Item.IconPath.trimmed().isEmpty() && QFileInfo(Item.IconPath).exists())
			ItemIcon = QIcon(Item.IconPath);
		else
			ItemIcon = style()->standardIcon(QStyle::SP_FileIcon);
		QAction* CustomAction = new QAction(ItemIcon, Item.Label.isEmpty() ? QString("Custom Item") : Item.Label, ToolBar);
		connect(CustomAction, &QAction::triggered, this, [this, Item]()
		{
			ExecuteToolbarScript(this, Item.FunctionScript);
		});
		ToolBar->addAction(CustomAction);
		HasAnyVisibleItems = true;
	}

	if (!AddedExtraToolsButton)
	{
		if (HasAnyVisibleItems)
			ToolBar->addSeparator();
		AddExtraToolsButton();
	}

	addToolBar(Qt::TopToolBarArea, ToolBar);
	mMainToolBar = ToolBar;

	for (QToolButton* Button : ToolBar->findChildren<QToolButton*>())
	{
		if (Button->objectName() == "ExtraToolsDropDownButton")
		{
			Button->setMinimumWidth(92);
			Button->setMaximumWidth(130);
			Button->setMinimumHeight(38);
			Button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
			continue;
		}
		Button->setMinimumWidth(66);
		Button->setMinimumHeight(38);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	}
}

void mlMainWindow::InitExport2BinGUI()
{
	QDockWidget *dock = new QDockWidget(this);
	dock->setWindowTitle("Export2Bin");
	dock->setFloating(true);

	QWidget* widget = new QWidget(dock);
	QGridLayout* gridLayout = new QGridLayout();
	widget->setLayout(gridLayout);
	dock->setWidget(widget);

	Export2BinGroupBox* groupBox = new Export2BinGroupBox(dock, this);
	gridLayout->addWidget(groupBox, 0, 0);

	QLabel* label = new QLabel("Drag Files Here", groupBox);
	label->setAlignment(Qt::AlignCenter);
	QVBoxLayout* groupBoxLayout = new QVBoxLayout(groupBox);
	groupBoxLayout->addWidget(label);
	groupBox->setLayout(groupBoxLayout);

	mExport2BinOverwriteWidget = new QCheckBox("&Overwrite Existing Files", widget);
	gridLayout->addWidget(mExport2BinOverwriteWidget, 1, 0);

	QSettings Settings;
	mExport2BinOverwriteWidget->setChecked(Settings.value("Export2Bin_OverwriteFiles", true).toBool());

	QHBoxLayout* dirLayout = new QHBoxLayout();
	QLabel* dirLabel = new QLabel("Ouput Directory:", widget);
	mExport2BinTargetDirWidget = new QLineEdit(widget);
	QToolButton* dirBrowseButton = new QToolButton(widget);
	dirBrowseButton->setText("...");

	const QDir defaultPath = QString("%1/model_export/export2bin/").arg(mToolsPath);
	mExport2BinTargetDirWidget->setText(Settings.value("Export2Bin_TargetDir", defaultPath.absolutePath()).toString());

	connect(dirBrowseButton, SIGNAL(clicked()), this, SLOT(OnExport2BinChooseDirectory()));
	connect(mExport2BinOverwriteWidget, SIGNAL(clicked()), this, SLOT(OnExport2BinToggleOverwriteFiles()));

	dirBrowseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	dirLayout->addWidget(dirLabel);
	dirLayout->addWidget(mExport2BinTargetDirWidget);
	dirLayout->addWidget(dirBrowseButton);

	gridLayout->addLayout(dirLayout, 2, 0);

	groupBox->setAcceptDrops(true);

	dock->resize(QSize(256, 256));

	mExport2BinGUIWidget = dock;
}

void mlMainWindow::closeEvent(QCloseEvent* Event)
{
	UpdateStatsTimers();
	FlushCurrentItemTime();
	QSettings Settings;
	Settings.beginGroup("MainWindow");
	Settings.setValue("Geometry", saveGeometry());
	Settings.setValue("State", saveState());
	Settings.endGroup();

	Event->accept();
}

bool mlMainWindow::eventFilter(QObject* Watched, QEvent* Event)
{
	switch (Event->type())
	{
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonDblClick:
	case QEvent::Wheel:
	case QEvent::KeyPress:
	case QEvent::TouchBegin:
	case QEvent::TouchUpdate:
	case QEvent::FocusIn:
		RecordUserActivity();
		break;
	default:
		break;
	}

	// Pencil hover overlay on category tab bar
	if (mCategoryTabs && mCategoryTabPencilButton)
	{
		if (Watched == mCategoryTabs)
		{
			if (Event->type() == QEvent::MouseMove)
			{
				const QMouseEvent* Me = static_cast<QMouseEvent*>(Event);
				const int TabIdx = mCategoryTabs->tabAt(Me->pos());
				mCategoryTabHoveredIndex = TabIdx;
				if (TabIdx >= 0 && !IsAddCategoryTabKey(mCategoryTabs->tabData(TabIdx).toString()))
				{
					const QRect TR = mCategoryTabs->tabRect(TabIdx);
					const int BtnW = 16;
					const int BtnH = 16;
					mCategoryTabPencilButton->setGeometry(TR.right() - BtnW - 1, TR.top() + (TR.height() - BtnH) / 2, BtnW, BtnH);
					mCategoryTabPencilButton->raise();
					mCategoryTabPencilButton->show();
				}
				else
				{
					mCategoryTabPencilButton->hide();
				}
			}
			else if (Event->type() == QEvent::Leave)
			{
				if (!mCategoryTabPencilButton->underMouse())
				{
					mCategoryTabPencilButton->hide();
					mCategoryTabHoveredIndex = -1;
				}
			}
		}
		else if (Watched == mCategoryTabPencilButton && Event->type() == QEvent::Leave)
		{
			mCategoryTabPencilButton->hide();
			mCategoryTabHoveredIndex = -1;
		}
	}

	if ((Watched == mOutputWidget || Watched == (mOutputWidget ? mOutputWidget->viewport() : NULL)) && Event->type() == QEvent::KeyPress)
	{
		QKeyEvent* KeyEvent = static_cast<QKeyEvent*>(Event);
		if (KeyEvent->matches(QKeySequence::SelectAll) && mOutputWidget)
		{
			mOutputWidget->selectAll();
			return true;
		}

		if (KeyEvent->matches(QKeySequence::Copy) && mOutputWidget)
		{
			QStringList SelectedBlocks;
			const QList<QTreeWidgetItem*> SelectedItems = mOutputWidget->selectedItems();
			for (QTreeWidgetItem* Item : SelectedItems)
			{
				const QString BlockText = LogBlockText(Item);
				if (!BlockText.isEmpty())
					SelectedBlocks.append(BlockText);
			}

			if (!SelectedBlocks.isEmpty())
				QApplication::clipboard()->setText(SelectedBlocks.join("\n\n"));
			return true;
		}
	}

	if ((Watched == (mFileListWidget ? mFileListWidget->viewport() : NULL)
		|| Watched == (mOutputWidget ? mOutputWidget->viewport() : NULL)
		|| Watched == (mOutputPlainWidget ? mOutputPlainWidget->viewport() : NULL))
		&& (Event->type() == QEvent::Resize || Event->type() == QEvent::Show))
	{
		UpdateBackgroundOverlays();
	}

	return QMainWindow::eventFilter(Watched, Event);
}

void mlMainWindow::SteamUpdate()
{
	SteamAPI_RunCallbacks();
	static int PollCounter = 0;
	if (++PollCounter >= 4)
	{
		UpdateGameRunningState();
		PollCounter = 0;
	}
}

bool mlMainWindow::IsTrackedGameProcessAlive() const
{
	if (!mGameProcessId)
		return false;

	HANDLE ProcessHandle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(mGameProcessId));
	if (!ProcessHandle)
		return false;

	const DWORD WaitResult = WaitForSingleObject(ProcessHandle, 0);
	CloseHandle(ProcessHandle);
	return WaitResult == WAIT_TIMEOUT;
}

mlMainWindow::GameRunningState mlMainWindow::DetectGameRunningState()
{
	if (IsTrackedGameProcessAlive())
	{
		GameWindowSearchState SearchState = { static_cast<DWORD>(mGameProcessId), false };
		EnumWindows(FindVisibleWindowForProcess, reinterpret_cast<LPARAM>(&SearchState));
		return SearchState.HasVisibleWindow ? GameRunning : GameStarting;
	}

	mGameProcessId = 0;
	HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (Snapshot == INVALID_HANDLE_VALUE)
		return GameNotRunning;

	PROCESSENTRY32 ProcessEntry;
	ProcessEntry.dwSize = sizeof(PROCESSENTRY32);
	GameRunningState State = GameNotRunning;
	if (Process32First(Snapshot, &ProcessEntry))
	{
		do
		{
			if (_stricmp(ProcessEntry.szExeFile, "BlackOps3.exe") == 0)
			{
				mGameProcessId = ProcessEntry.th32ProcessID;
				GameWindowSearchState SearchState = { ProcessEntry.th32ProcessID, false };
				EnumWindows(FindVisibleWindowForProcess, reinterpret_cast<LPARAM>(&SearchState));
				State = SearchState.HasVisibleWindow ? GameRunning : GameStarting;
				break;
			}
		} while (Process32Next(Snapshot, &ProcessEntry));
	}

	CloseHandle(Snapshot);
	return State;
}

bool mlMainWindow::IsGameRunning() const
{
	return mGameRunningState != GameNotRunning;
}

void mlMainWindow::UpdateGameRunningState()
{
	if (!mCloseGameButton || !mCloseGameStatusLabel)
		return;

	const GameRunningState PreviousState = mGameRunningState;
	mGameRunningState = DetectGameRunningState();
	const qint64 NowMs = mStatsElapsedTimer.isValid() ? mStatsElapsedTimer.elapsed() : 0;
	const bool WaitingForRecentLaunch = mPendingLaunchRequestedMs > 0 && NowMs > 0 && NowMs - mPendingLaunchRequestedMs < 120000;
	if (mGameRunningState == GameNotRunning && WaitingForRecentLaunch)
		mGameRunningState = GameStarting;
	if (mGameRunningState != GameNotRunning)
		mPendingLaunchRequestedMs = 0;
	FinalizeGameStatsTransition(PreviousState, mGameRunningState);
	const bool Running = (mGameRunningState != GameNotRunning);
	mCachedGameRunning = Running;
	mCloseGameButton->setEnabled(Running);

	if (mGameRunningState == GameRunning)
	{
		mCloseGameButton->setText("Close Game");
		mCloseGameButton->setEnabled(true);
		mCloseGameStatusLabel->setText("Game Running!");
		mCloseGameStatusLabel->setStyleSheet("color: #44d17a; font-weight: 700;");
		CloseOnlineLaunchProgressDialog();
	}
	else if (mGameRunningState == GameStarting)
	{
		mCloseGameButton->setText("Close Game");
		mCloseGameButton->setEnabled(true);
		mCloseGameStatusLabel->setText("Game starting up...");
		mCloseGameStatusLabel->setStyleSheet("color: #ff9b42;");
		if (mPendingOnlineLaunchFeedback && mLaunchProgressDialog)
		{
			mLaunchProgressDialog->setLabelText(
				QString("Steam launch requested for %1...\n\nBlack Ops 3 is starting up now. This window will close automatically once the game is fully open.")
					.arg(mPendingOnlineLaunchLabel.isEmpty() ? QString("selected content") : mPendingOnlineLaunchLabel));
			mLaunchProgressDialog->show();
		}
	}
	else
	{
		mPendingLaunchRequestedMs = 0;
		if (mPendingOnlineLaunchFeedback && mLaunchProgressDialog)
		{
			mLaunchProgressDialog->setLabelText(
				QString("Waiting for Steam to start %1...\n\nIf this takes longer than expected, Steam may still be processing the online launch request.")
					.arg(mPendingOnlineLaunchLabel.isEmpty() ? QString("selected content") : mPendingOnlineLaunchLabel));
			mLaunchProgressDialog->show();
		}
		QTreeWidgetItem* CurrentItem = mFileListWidget ? mFileListWidget->currentItem() : NULL;
		const int ItemType = CurrentItem ? CurrentItem->data(0, Qt::UserRole).toInt() : ML_ITEM_UNKNOWN;
		const bool CurrentItemChecked = CurrentItem && CurrentItem->data(0, ML_ITEM_CHECKSTATE_ROLE).toInt() == Qt::Checked;
		if (ItemType == ML_ITEM_MAP)
		{
			mCloseGameButton->setText("Start Map");
			mCloseGameButton->setEnabled(CurrentItemChecked);
			mCloseGameStatusLabel->setText(CurrentItemChecked ? "Launch Map" : "Check the selected map to launch it");
		}
		else if (ItemType == ML_ITEM_MOD || ItemType == ML_ITEM_MOD_GROUP)
		{
			mCloseGameButton->setText("Start Mod");
			mCloseGameButton->setEnabled(CurrentItemChecked);
			mCloseGameStatusLabel->setText(CurrentItemChecked ? "Launch Mod" : "Check the selected mod to launch it");
		}
		else
		{
			mCloseGameButton->setText("Start Game");
			mCloseGameButton->setEnabled(false);
			mCloseGameStatusLabel->setText("Select and check a map or mod to launch it");
		}
		mCloseGameStatusLabel->setStyleSheet(QString());
	}
}

QString mlMainWindow::ItemContentRoot(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	Q_UNUSED(EntryName);
	if (ItemType == ML_ITEM_MAP)
		return QDir::cleanPath(QString("%1/usermaps/%2").arg(mGamePath, ContainerName));
	if (ItemType == ML_ITEM_MOD || ItemType == ML_ITEM_MOD_GROUP)
		return QDir::cleanPath(QString("%1/mods/%2").arg(mGamePath, ContainerName));
	return QString();
}

quint64 mlMainWindow::ItemDiskSizeBytes(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	const QString RootPath = ItemContentRoot(ItemType, ContainerName, EntryName);
	if (RootPath.isEmpty())
		return 0;
	return DirectorySizeBytes(RootPath);
}

void mlMainWindow::QueueGameStatsForItem(const QString& FavoriteKey)
{
	mPendingGameStatsItemKey = NormalizeStatsItemKey(FavoriteKey);
	mPendingGameQueuedMs = (mStatsElapsedTimer.isValid() && !mPendingGameStatsItemKey.isEmpty()) ? mStatsElapsedTimer.elapsed() : 0;
}

void mlMainWindow::FinalizeGameStatsTransition(GameRunningState PreviousState, GameRunningState CurrentState)
{
	const qint64 NowMs = mStatsElapsedTimer.isValid() ? mStatsElapsedTimer.elapsed() : 0;
	if (CurrentState == GameNotRunning && !mPendingGameStatsItemKey.isEmpty() && mPendingGameQueuedMs > 0 && NowMs > mPendingGameQueuedMs + 120000)
	{
		mPendingGameStatsItemKey.clear();
		mPendingGameQueuedMs = 0;
	}

	if (PreviousState == GameNotRunning && CurrentState != GameNotRunning)
	{
		mCurrentGameStartedMs = NowMs;
		if (!mPendingGameStatsItemKey.isEmpty())
		{
			mActiveGameStatsItemKey = mPendingGameStatsItemKey;
			mPendingGameStatsItemKey.clear();
			mPendingGameQueuedMs = 0;
			IncrementStat("Stats/TotalGameLaunches");
			IncrementStat(QString("Stats/ItemsPlayCount/%1").arg(mActiveGameStatsItemKey));
			QSettings().setValue(QString("Stats/ItemsLastPlayedUtc/%1").arg(mActiveGameStatsItemKey), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
		}
		else
			mActiveGameStatsItemKey.clear();
	}

	if (PreviousState != GameNotRunning && CurrentState == GameNotRunning)
	{
		const qint64 DurationSeconds = (mCurrentGameStartedMs > 0 && NowMs > mCurrentGameStartedMs) ? ((NowMs - mCurrentGameStartedMs) / 1000) : 0;
		AddStatSeconds("Stats/TotalGameSeconds", DurationSeconds);
		if (!mActiveGameStatsItemKey.isEmpty())
			AddStatSeconds(QString("Stats/ItemsGameSeconds/%1").arg(mActiveGameStatsItemKey), DurationSeconds);
		mCurrentGameStartedMs = 0;
		mActiveGameStatsItemKey.clear();
	}
}

bool mlMainWindow::ShouldUseSteamOnlineLaunch() const
{
	if (mRunOnlineWidget && mRunOnlineWidget->isChecked())
		return true;
	return mQuickLaunchWidget && mQuickLaunchWidget->IsWorkshopSelection();
}

QString mlMainWindow::SectionSettingKey(const QString& SectionName) const
{
	QString Normalized = SectionName.toLower();
	Normalized.replace(' ', '_');
	return QString("Sections/%1").arg(Normalized);
}

QString mlMainWindow::LauncherDataRoot() const
{
	return QDir::cleanPath(QString("%1/modtools/launcher_data").arg(mGamePath));
}

QString mlMainWindow::DefaultScriptsManifestPath() const
{
	return QDir::cleanPath(QString("%1/modtools/ModLauncherSrc/resources/default_scripts.txt").arg(mGamePath));
}

QString mlMainWindow::DefaultUiManifestPath() const
{
	return QDir::cleanPath(QString("%1/modtools/ModLauncherSrc/resources/default_ui.txt").arg(mGamePath));
}

bool mlMainWindow::EnsureDefaultScriptsManifest(QString* Error) const
{
	const QString SourceManifestPath = DefaultScriptsManifestPath();
	if (QFileInfo(SourceManifestPath).isFile() || QFile::exists(":/resources/default_scripts.txt"))
		return true;

	const QString ManifestPath = QDir::cleanPath(QString("%1/default_scripts.txt").arg(LauncherDataRoot()));
	if (!QDir().mkpath(QFileInfo(ManifestPath).absolutePath()))
	{
		if (Error)
			*Error = QString("Unable to create '%1'.").arg(QFileInfo(ManifestPath).absolutePath());
		return false;
	}

	QStringList Entries;
	const QString RootPath = QDir::cleanPath(QString("%1/share/raw/scripts").arg(mGamePath));
	QDir RootDir(RootPath);
	if (RootDir.exists())
	{
		QDirIterator It(RootPath, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
		while (It.hasNext())
		{
			const QString FilePath = It.next();
			const QString RelativePath = NormalizeDefaultManifestPath(QDir(mGamePath).relativeFilePath(FilePath));
			if (!RelativePath.isEmpty())
				Entries << RelativePath;
		}

		QDirIterator NamespaceIt(RootPath, QStringList() << "*.gsc" << "*.csc", QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
		while (NamespaceIt.hasNext())
		{
			const QString FilePath = NamespaceIt.next();
			QFile ScriptFile(FilePath);
			if (!ScriptFile.open(QIODevice::ReadOnly))
				continue;

			const QString RelativePath = NormalizeDefaultManifestPath(QDir(mGamePath).relativeFilePath(FilePath));
			for (const QString& NamespaceName : ExtractDeclaredNamespaces(QString::fromUtf8(ScriptFile.readAll())))
				Entries << QString("namespace:%1=%2").arg(NamespaceName, RelativePath);
		}
	}

	Entries.removeDuplicates();
	Entries.sort();

	QFile ManifestFile(ManifestPath);
	if (!ManifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		if (Error)
			*Error = QString("Unable to write '%1'.").arg(QDir::toNativeSeparators(ManifestPath));
		return false;
	}

	QTextStream Stream(&ManifestFile);
	for (const QString& Entry : Entries)
		Stream << Entry << "\n";
	return true;
}

bool mlMainWindow::EnsureDefaultUiManifest(QString* Error) const
{
	const QString SourceManifestPath = DefaultUiManifestPath();
	if (QFileInfo(SourceManifestPath).isFile() || QFile::exists(":/resources/default_ui.txt"))
		return true;

	const QString ManifestPath = QDir::cleanPath(QString("%1/default_ui.txt").arg(LauncherDataRoot()));
	if (!QDir().mkpath(QFileInfo(ManifestPath).absolutePath()))
	{
		if (Error)
			*Error = QString("Unable to create '%1'.").arg(QFileInfo(ManifestPath).absolutePath());
		return false;
	}

	QStringList Entries;
	const QStringList RootPaths = QStringList()
		<< QDir::cleanPath(QString("%1/share/raw/ui").arg(mGamePath))
		<< QDir::cleanPath(QString("%1/share/raw/scripts/ui").arg(mGamePath))
		<< QDir::cleanPath(QString("%1/share/raw/scripts/lua").arg(mGamePath));
	for (const QString& RootPath : RootPaths)
	{
		QDir RootDir(RootPath);
		if (!RootDir.exists())
			continue;

		QDirIterator It(RootPath, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
		while (It.hasNext())
		{
			const QString FilePath = It.next();
			const QString RelativePath = CanonicalDefaultUiManifestEntry(QDir(mGamePath).relativeFilePath(FilePath));
			if (!RelativePath.isEmpty())
				Entries << RelativePath;
		}
	}

	Entries.removeDuplicates();
	Entries.sort();

	QFile ManifestFile(ManifestPath);
	if (!ManifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		if (Error)
			*Error = QString("Unable to write '%1'.").arg(QDir::toNativeSeparators(ManifestPath));
		return false;
	}

	QTextStream Stream(&ManifestFile);
	for (const QString& Entry : Entries)
		Stream << Entry << "\n";
	return true;
}

QSet<QString> mlMainWindow::LoadDefaultScriptsManifest(QString* Error) const
{
	if (!EnsureDefaultScriptsManifest(Error))
		return QSet<QString>();

	QString ManifestPath = DefaultScriptsManifestPath();
	if (!QFileInfo(ManifestPath).isFile())
	{
		if (QFile::exists(":/resources/default_scripts.txt"))
			ManifestPath = ":/resources/default_scripts.txt";
		else
			ManifestPath = QDir::cleanPath(QString("%1/default_scripts.txt").arg(LauncherDataRoot()));
	}

	QFile ManifestFile(ManifestPath);
	if (!ManifestFile.open(QIODevice::ReadOnly))
	{
		if (Error)
			*Error = QString("Unable to open '%1'.").arg(QDir::toNativeSeparators(ManifestPath));
		return QSet<QString>();
	}

	QSet<QString> Entries;
	while (!ManifestFile.atEnd())
	{
		const QString Entry = NormalizeDefaultManifestPath(QString::fromUtf8(ManifestFile.readLine()).trimmed());
		if (!Entry.isEmpty() && !IsNamespaceManifestEntry(Entry))
			Entries.insert(Entry);
	}
	return Entries;
}

QSet<QString> mlMainWindow::LoadDefaultUiManifest(QString* Error) const
{
	if (!EnsureDefaultUiManifest(Error))
		return QSet<QString>();

	QString ManifestPath = DefaultUiManifestPath();
	if (!QFileInfo(ManifestPath).isFile())
	{
		if (QFile::exists(":/resources/default_ui.txt"))
			ManifestPath = ":/resources/default_ui.txt";
		else
			ManifestPath = QDir::cleanPath(QString("%1/default_ui.txt").arg(LauncherDataRoot()));
	}

	QFile ManifestFile(ManifestPath);
	if (!ManifestFile.open(QIODevice::ReadOnly))
	{
		if (Error)
			*Error = QString("Unable to open '%1'.").arg(QDir::toNativeSeparators(ManifestPath));
		return QSet<QString>();
	}

	QSet<QString> Entries;
	while (!ManifestFile.atEnd())
	{
		const QString Entry = NormalizeDefaultManifestPath(QString::fromUtf8(ManifestFile.readLine()).trimmed());
		if (!Entry.isEmpty())
			Entries.insert(Entry);
	}
	return Entries;
}

QString mlMainWindow::NotesFilePath(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	const bool IsMap = (ItemType == ML_ITEM_MAP);
	const QString TargetName = SanitizedFileName(IsMap ? EntryName : ContainerName);
	return QDir::cleanPath(QString("%1/notes/%2/%3.json").arg(LauncherDataRoot(), IsMap ? "maps" : "mods", TargetName));
}

QString mlMainWindow::WorkshopVersionsRoot(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	const bool IsMap = (ItemType == ML_ITEM_MAP);
	const QString TargetName = SanitizedFileName(IsMap ? EntryName : ContainerName);
	return QDir::cleanPath(QString("%1/workshop_versions/%2/%3").arg(LauncherDataRoot(), IsMap ? "maps" : "mods", TargetName));
}

QString mlMainWindow::ActiveWorkshopFolder(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	if (ItemType == ML_ITEM_MAP)
		return QDir::cleanPath(QString("%1/usermaps/%2/zone").arg(mGamePath, EntryName));

	return QDir::cleanPath(QString("%1/mods/%2/zone").arg(mGamePath, ContainerName));
}

bool mlMainWindow::EditNotesForItem(QTreeWidgetItem* Item)
{
	if (!Item)
		return false;

	const int ItemType = Item->data(0, Qt::UserRole).toInt();
	if (ItemType != ML_ITEM_MAP && ItemType != ML_ITEM_MOD && ItemType != ML_ITEM_MOD_GROUP)
		return false;

	const QString ContainerName = GetItemContainerName(Item);
	const QString EntryName = GetItemEntryName(Item);
	const QString DisplayName = (ItemType == ML_ITEM_MAP) ? EntryName : ContainerName;
	const QString FilePath = NotesFilePath(ItemType, ContainerName, EntryName);

	QJsonObject ExistingRoot;
	QFile ExistingFile(FilePath);
	if (ExistingFile.open(QIODevice::ReadOnly))
		ExistingRoot = QJsonDocument::fromJson(ExistingFile.readAll()).object();

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle(QString("Notes - %1").arg(DisplayName));
	Dialog.resize(760, 620);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QTabWidget* Tabs = new QTabWidget(&Dialog);
	Layout->addWidget(Tabs, 1);

	auto MakeTab = [&](const QString& Title, const QString& Key) -> QTextEdit*
	{
		QTextEdit* Editor = new QTextEdit(&Dialog);
		Editor->setAcceptRichText(false);
		Editor->setPlainText(ExistingRoot.value(Key).toString());
		Tabs->addTab(Editor, Title);
		return Editor;
	};

	QTextEdit* GeneralNotes = MakeTab("General", "general");
	QTextEdit* CompileNotes = MakeTab("Compile", "compile");
	QTextEdit* PublishNotes = MakeTab("Publish", "publish");

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	Layout->addWidget(ButtonBox);
	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return false;

	QDir().mkpath(QFileInfo(FilePath).absolutePath());
	QJsonObject Root;
	Root["general"] = GeneralNotes->toPlainText().trimmed();
	Root["compile"] = CompileNotes->toPlainText().trimmed();
	Root["publish"] = PublishNotes->toPlainText().trimmed();
	Root["updated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
	QFile File(FilePath);
	if (!File.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, "Notes", QString("Unable to save notes to '%1'.").arg(FilePath));
		return false;
	}

	File.write(QJsonDocument(Root).toJson(QJsonDocument::Indented));
	return true;
}

void mlMainWindow::ShowItemInformationDialog(QTreeWidgetItem* Item)
{
	if (!Item)
		return;

	const int ItemType = Item->data(0, Qt::UserRole).toInt();
	if (ItemType == ML_ITEM_UNKNOWN)
		return;

	const QString ContainerName = GetItemContainerName(Item);
	const QString EntryName = GetItemEntryName(Item);
	const QString FavoriteKey = NormalizeStatsItemKey(GetItemFavoriteKey(Item));
	const QString DisplayName = DisplayNameForEntry(FavoriteKey);
	const QString InternalName = (ItemType == ML_ITEM_MAP) ? EntryName : ContainerName;
	const QString TypeLabel = (ItemType == ML_ITEM_MAP) ? "Map" : "Mod";
	const QString ContentRoot = ItemContentRoot(ItemType, ContainerName, EntryName);
	const quint64 SizeBytes = ItemDiskSizeBytes(ItemType, ContainerName, EntryName);
	const QString NotesPath = NotesFilePath(ItemType, ContainerName, EntryName);
	const QString WorkshopRoot = ActiveWorkshopFolder(ItemType, ContainerName, EntryName);
	const QString WorkshopJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(WorkshopRoot));
	const QString VersionsRoot = WorkshopVersionsRoot(ItemType, ContainerName, EntryName);
	const QStringList VersionFolders = QDir(VersionsRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

	QJsonObject NotesRoot;
	QFile NotesFile(NotesPath);
	if (NotesFile.open(QIODevice::ReadOnly))
		NotesRoot = QJsonDocument::fromJson(NotesFile.readAll()).object();

	QJsonObject WorkshopRootObject;
	QFile WorkshopFile(WorkshopJsonPath);
	if (WorkshopFile.open(QIODevice::ReadOnly))
		WorkshopRootObject = QJsonDocument::fromJson(WorkshopFile.readAll()).object();

	const qint64 PlayCount = StatValue(QString("Stats/ItemsPlayCount/%1").arg(FavoriteKey));
	const qint64 GameSeconds = StatValue(QString("Stats/ItemsGameSeconds/%1").arg(FavoriteKey));
	const qint64 LauncherSeconds = StatValue(QString("Stats/Items/%1/LauncherSeconds").arg(FavoriteKey));
	const qint64 CompileCount = StatValue(QString("Stats/ItemsCompileCount/%1").arg(FavoriteKey));
	const qint64 CompileSeconds = StatValue(QString("Stats/ItemsCompileSeconds/%1").arg(FavoriteKey));
	const qint64 LinkCount = StatValue(QString("Stats/ItemsLinkCount/%1").arg(FavoriteKey));
	const qint64 LinkSeconds = StatValue(QString("Stats/ItemsLinkSeconds/%1").arg(FavoriteKey));
	const qint64 PublishCount = StatValue(QString("Stats/ItemsPublishCount/%1").arg(FavoriteKey));
	const QString LastCompileUtc = QSettings().value(QString("Stats/ItemsLastCompileUtc/%1").arg(FavoriteKey)).toString();
	const QString LastLinkUtc = QSettings().value(QString("Stats/ItemsLastLinkUtc/%1").arg(FavoriteKey)).toString();
	const QString LastPlayedUtc = QSettings().value(QString("Stats/ItemsLastPlayedUtc/%1").arg(FavoriteKey)).toString();

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle(QString("%1 Information").arg(DisplayTextForItem(ItemType, ContainerName, EntryName)));
	Dialog.resize(760, 680);
	Dialog.setMinimumSize(700, 620);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QLabel* HeadingLabel = new QLabel(QString("%1 overview").arg(TypeLabel), &Dialog);
	QFont HeadingFont = HeadingLabel->font();
	HeadingFont.setBold(true);
	HeadingFont.setPointSize(HeadingFont.pointSize() + 3);
	HeadingLabel->setFont(HeadingFont);
	Layout->addWidget(HeadingLabel);

	QFrame* TopDivider = new QFrame(&Dialog);
	TopDivider->setFrameShape(QFrame::HLine);
	Layout->addWidget(TopDivider);

	QGridLayout* Grid = new QGridLayout();
	Grid->setColumnStretch(1, 1);
	int Row = 0;
	auto AddInfoRow = [&](const QString& Label, const QString& Value)
	{
		QLabel* KeyLabel = new QLabel(Label, &Dialog);
		QFont LabelFont = KeyLabel->font();
		LabelFont.setBold(true);
		KeyLabel->setFont(LabelFont);
		QLabel* ValueLabel = new QLabel(Value.isEmpty() ? "-" : Value, &Dialog);
		ValueLabel->setWordWrap(true);
		ValueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Grid->addWidget(KeyLabel, Row, 0, Qt::AlignTop);
		Grid->addWidget(ValueLabel, Row, 1, Qt::AlignTop);
		Row++;
	};
	auto FormatUtc = [&](const QString& Value) -> QString
	{
		const QDateTime Timestamp = QDateTime::fromString(Value, Qt::ISODate);
		return Timestamp.isValid() ? Timestamp.toLocalTime().toString("yyyy-MM-dd hh:mm:ss") : QString();
	};

	AddInfoRow("Name", InternalName);
	AddInfoRow("Display Name", DisplayName);
	AddInfoRow("Type", TypeLabel);
	AddInfoRow("Size", FormatDataSizeBytes(SizeBytes));
	AddInfoRow("Time Wasted", FormatDuration(LauncherSeconds));
	AddInfoRow("Plays", QString::number(PlayCount));
	AddInfoRow("Time In Game", FormatDuration(GameSeconds));
	AddInfoRow("Compile Time", QString("%1 across %2 build(s)").arg(FormatDuration(CompileSeconds)).arg(CompileCount));
	AddInfoRow("Link Time", QString("%1 across %2 link(s)").arg(FormatDuration(LinkSeconds)).arg(LinkCount));
	AddInfoRow("Workshop Publishes", QString::number(PublishCount));
	AddInfoRow("Workshop Versions", QString::number(VersionFolders.count()));
	AddInfoRow("Workshop Title", WorkshopRootObject.value("Title").toString());
	AddInfoRow("Workshop File ID", WorkshopRootObject.value("PublisherID").toString());
	AddInfoRow("Last Compile", FormatUtc(LastCompileUtc));
	AddInfoRow("Last Link", FormatUtc(LastLinkUtc));
	AddInfoRow("Last Played", FormatUtc(LastPlayedUtc));
	AddInfoRow("Content Folder", QDir::toNativeSeparators(ContentRoot));
	Layout->addLayout(Grid);

	QFrame* BottomDivider = new QFrame(&Dialog);
	BottomDivider->setFrameShape(QFrame::HLine);
	Layout->addWidget(BottomDivider);

	QLabel* NotesLabel = new QLabel("Notes", &Dialog);
	QFont NotesFont = NotesLabel->font();
	NotesFont.setBold(true);
	NotesLabel->setFont(NotesFont);
	Layout->addWidget(NotesLabel);

	QTextEdit* NotesPreview = new QTextEdit(&Dialog);
	NotesPreview->setReadOnly(true);
	NotesPreview->setAcceptRichText(false);
	const QString InitialNotesText = (QStringList()
		<< NotesRoot.value("general").toString().trimmed()
		<< NotesRoot.value("compile").toString().trimmed()
		<< NotesRoot.value("publish").toString().trimmed()).join("\n\n").trimmed();
	NotesPreview->setPlainText(InitialNotesText);
	if (NotesPreview->toPlainText().trimmed().isEmpty())
		NotesPreview->setPlainText("No notes saved yet.");
	Layout->addWidget(NotesPreview, 1);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Close, &Dialog);
	QPushButton* EditNotesButton = ButtonBox->addButton("Edit Notes...", QDialogButtonBox::ActionRole);
	connect(EditNotesButton, &QPushButton::clicked, this, [=]()
	{
		if (!EditNotesForItem(Item))
			return;
		QFile RefreshedNotesFile(NotesPath);
		if (!RefreshedNotesFile.open(QIODevice::ReadOnly))
			return;
		const QJsonObject RefreshedNotes = QJsonDocument::fromJson(RefreshedNotesFile.readAll()).object();
		const QString RefreshedNotesText = (QStringList()
			<< RefreshedNotes.value("general").toString().trimmed()
			<< RefreshedNotes.value("compile").toString().trimmed()
			<< RefreshedNotes.value("publish").toString().trimmed()).join("\n\n").trimmed();
		NotesPreview->setPlainText(RefreshedNotesText);
		if (NotesPreview->toPlainText().trimmed().isEmpty())
			NotesPreview->setPlainText("No notes saved yet.");
	});
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	Layout->addWidget(ButtonBox);

	Dialog.exec();
}

bool mlMainWindow::SaveWorkshopVersionSnapshot(int ItemType, const QString& ContainerName, const QString& EntryName, const QString& SourceWorkshopJsonPath, const QString& VersionLabel, const QString& OverridePublisherId)
{
	QFile SourceFile(SourceWorkshopJsonPath);
	if (!SourceFile.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(this, "Workshop Versions", QString("Unable to open '%1'.").arg(SourceWorkshopJsonPath));
		return false;
	}

	QJsonObject Root = QJsonDocument::fromJson(SourceFile.readAll()).object();
	const QString PublisherId = OverridePublisherId.trimmed().isEmpty() ? Root.value("PublisherID").toString().trimmed() : OverridePublisherId.trimmed();
	const QString FolderName = SanitizedFileName(VersionLabel.isEmpty() ? (PublisherId.isEmpty() ? QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") : PublisherId) : VersionLabel);
	const QString VersionFolder = QDir::cleanPath(QString("%1/%2").arg(WorkshopVersionsRoot(ItemType, ContainerName, EntryName), FolderName));
	QDir().mkpath(VersionFolder);
	Root["LauncherVersionLabel"] = VersionLabel.trimmed().isEmpty() ? HumanizedVersionLabel(FolderName) : VersionLabel.trimmed();

	QString ThumbnailPath = Root.value("Thumbnail").toString().trimmed();
	if (!ThumbnailPath.isEmpty() && QFileInfo(ThumbnailPath).isRelative())
		ThumbnailPath = QDir(QFileInfo(SourceWorkshopJsonPath).absolutePath()).filePath(ThumbnailPath);
	if (!ThumbnailPath.isEmpty() && !QFileInfo(ThumbnailPath).isFile())
	{
		const QString FallbackThumbPath = QDir(QFileInfo(SourceWorkshopJsonPath).absolutePath()).filePath(QFileInfo(ThumbnailPath).fileName());
		if (QFileInfo(FallbackThumbPath).isFile())
			ThumbnailPath = FallbackThumbPath;
	}
	if (!ThumbnailPath.isEmpty() && !QFileInfo(ThumbnailPath).isFile())
	{
		const QString ReplacementThumb = QFileDialog::getOpenFileName(this, "Select Thumbnail For Workshop Version", QFileInfo(SourceWorkshopJsonPath).absolutePath(), "Images (*.png *.jpg *.jpeg *.gif *.bmp)");
		if (!ReplacementThumb.isEmpty())
			ThumbnailPath = ReplacementThumb;
	}

	if (!ThumbnailPath.isEmpty() && QFileInfo(ThumbnailPath).isFile())
	{
		const QString ThumbFileName = QFileInfo(ThumbnailPath).fileName();
		QFile::remove(QDir::cleanPath(QString("%1/%2").arg(VersionFolder, ThumbFileName)));
		QFile::copy(ThumbnailPath, QDir::cleanPath(QString("%1/%2").arg(VersionFolder, ThumbFileName)));
		Root["Thumbnail"] = ThumbFileName;
	}

	if (!PublisherId.isEmpty())
		Root["PublisherID"] = PublisherId;

	QFile OutFile(QDir::cleanPath(QString("%1/workshop.json").arg(VersionFolder)));
	if (!OutFile.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, "Workshop Versions", QString("Unable to save version into '%1'.").arg(VersionFolder));
		return false;
	}

	OutFile.write(QJsonDocument(Root).toJson(QJsonDocument::Indented));
	return true;
}

bool mlMainWindow::ActivateWorkshopVersion(int ItemType, const QString& ContainerName, const QString& EntryName, const QString& VersionFolderPath)
{
	const QString SourceJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(VersionFolderPath));
	QFile SourceFile(SourceJsonPath);
	if (!SourceFile.open(QIODevice::ReadOnly))
		return false;

	QJsonObject Root = QJsonDocument::fromJson(SourceFile.readAll()).object();
	const QString TargetFolder = ActiveWorkshopFolder(ItemType, ContainerName, EntryName);
	QDir().mkpath(TargetFolder);

	const QString StoredThumbnail = Root.value("Thumbnail").toString().trimmed();
	if (!StoredThumbnail.isEmpty())
	{
		const QString StoredThumbnailPath = QDir::cleanPath(QString("%1/%2").arg(VersionFolderPath, StoredThumbnail));
		if (QFileInfo(StoredThumbnailPath).isFile())
		{
			const QString TargetThumbnailPath = QDir::cleanPath(QString("%1/%2").arg(TargetFolder, QFileInfo(StoredThumbnail).fileName()));
			QFile::remove(TargetThumbnailPath);
			QFile::copy(StoredThumbnailPath, TargetThumbnailPath);
			Root["Thumbnail"] = TargetThumbnailPath;
		}
	}

	QFile TargetFile(QDir::cleanPath(QString("%1/workshop.json").arg(TargetFolder)));
	if (!TargetFile.open(QIODevice::WriteOnly))
		return false;

	TargetFile.write(QJsonDocument(Root).toJson(QJsonDocument::Indented));
	return true;
}

void mlMainWindow::ShowWorkshopVersionsDialog(QTreeWidgetItem* Item)
{
	if (!Item)
		return;

	const int RawItemType = Item->data(0, Qt::UserRole).toInt();
	const int ItemType = (RawItemType == ML_ITEM_MAP) ? ML_ITEM_MAP : ML_ITEM_MOD_GROUP;
	const QString ContainerName = GetItemContainerName(Item);
	const QString EntryName = GetItemEntryName(Item);
	const QString ActiveFolder = ActiveWorkshopFolder(ItemType, ContainerName, EntryName);
	const QString VersionsRoot = WorkshopVersionsRoot(ItemType, ContainerName, EntryName);
	QDir().mkpath(VersionsRoot);

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Workshop Versions");
	Dialog.resize(760, 480);
	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	const QString FavoriteKey = (ItemType == ML_ITEM_MAP)
		? QString("map:%1").arg(EntryName.toLower())
		: QString("mod:%1").arg(ContainerName.toLower());
	const QString InternalName = (ItemType == ML_ITEM_MAP) ? EntryName : ContainerName;
	const QString DisplayName = DisplayNameForEntry(FavoriteKey);
	const QString HeaderName = DisplayName.isEmpty() ? InternalName : QString("%1 [%2]").arg(DisplayName, InternalName);
	Layout->addWidget(new QLabel(QString("Backups for %1").arg(HeaderName)));
	QListWidget* VersionsList = new QListWidget(&Dialog);
	VersionsList->setObjectName("WorkshopVersionsList");
	VersionsList->setSpacing(6);
	VersionsList->setMouseTracking(true);
	VersionsList->setSelectionMode(QAbstractItemView::SingleSelection);
	VersionsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	VersionsList->setFocusPolicy(Qt::NoFocus);
	Layout->addWidget(VersionsList, 1);

	auto ReadWorkshopObject = [](const QString& WorkshopJsonPath) -> QJsonObject
	{
		QFile WorkshopFile(WorkshopJsonPath);
		if (!WorkshopFile.open(QIODevice::ReadOnly))
			return QJsonObject();
		return QJsonDocument::fromJson(WorkshopFile.readAll()).object();
	};

	auto WorkshopVersionIsActive = [&](const QString& FolderPath) -> bool
	{
		const QString ActiveJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(ActiveFolder));
		const QString VersionJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(FolderPath));
		QFile ActiveFile(ActiveJsonPath);
		QFile VersionFile(VersionJsonPath);
		if (!ActiveFile.open(QIODevice::ReadOnly) || !VersionFile.open(QIODevice::ReadOnly))
			return false;
		return ActiveFile.readAll() == VersionFile.readAll();
	};

	auto RefreshVersions = [&]()
	{
		VersionsList->clear();
		const QStringList Folders = QDir(VersionsRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
		QString ActiveFolderPath;
		for (const QString& Folder : Folders)
		{
			const QString FolderPath = QDir::cleanPath(QString("%1/%2").arg(VersionsRoot, Folder));
			QListWidgetItem* ListItem = new QListWidgetItem(VersionsList);
			ListItem->setData(Qt::UserRole, FolderPath);
			ListItem->setSizeHint(QSize(0, 96));

			const QJsonObject Root = ReadWorkshopObject(QDir::cleanPath(QString("%1/workshop.json").arg(FolderPath)));
			const bool IsActive = WorkshopVersionIsActive(FolderPath);
			if (IsActive)
				ActiveFolderPath = FolderPath;

			QWidget* RowWidget = new QWidget(VersionsList);
			RowWidget->setObjectName(IsActive ? "WorkshopVersionRowActive" : "WorkshopVersionRow");
			QHBoxLayout* RowLayout = new QHBoxLayout(RowWidget);
			RowLayout->setContentsMargins(12, 12, 12, 12);
			RowLayout->setSpacing(12);

			QLabel* Thumb = new QLabel(RowWidget);
			Thumb->setFixedSize(64, 64);
			Thumb->setObjectName("BackgroundPreview");
			const QString ThumbPath = QDir::cleanPath(QString("%1/%2").arg(FolderPath, Root.value("Thumbnail").toString()));
			UpdateBackgroundPreviewLabel(Thumb, ThumbPath);
			RowLayout->addWidget(Thumb);

			QVBoxLayout* TextLayout = new QVBoxLayout();
			TextLayout->setContentsMargins(0, 0, 0, 0);
			TextLayout->setSpacing(4);
			const QString VersionDisplayLabel = Root.value("LauncherVersionLabel").toString().trimmed().isEmpty()
				? HumanizedVersionLabel(Folder)
				: Root.value("LauncherVersionLabel").toString().trimmed();
			QLabel* VersionName = new QLabel(QString("Version: %1").arg(VersionDisplayLabel), RowWidget);
			VersionName->setObjectName("WorkshopVersionTitleLabel");
			VersionName->setStyleSheet("font-weight:700;");
			TextLayout->addWidget(VersionName);
			QLabel* IdLabel = new QLabel(QString("Workshop ID: %1").arg(Root.value("PublisherID").toString().isEmpty() ? "none" : Root.value("PublisherID").toString()), RowWidget);
			IdLabel->setObjectName("WorkshopVersionIdLabel");
			TextLayout->addWidget(IdLabel);
			RowLayout->addLayout(TextLayout, 1);

			if (IsActive)
			{
				QLabel* ActiveLabel = new QLabel("ACTIVE", RowWidget);
				ActiveLabel->setObjectName("WorkshopVersionActiveBadge");
				RowLayout->addWidget(ActiveLabel, 0, Qt::AlignTop);
			}

			VersionsList->addItem(ListItem);
			VersionsList->setItemWidget(ListItem, RowWidget);
		}

		if (!ActiveFolderPath.isEmpty())
		{
			for (int Idx = 0; Idx < VersionsList->count(); Idx++)
			{
				QListWidgetItem* ListItem = VersionsList->item(Idx);
				if (ListItem && ListItem->data(Qt::UserRole).toString() == ActiveFolderPath)
				{
					VersionsList->setCurrentItem(ListItem);
					break;
				}
			}
		}
	};
	RefreshVersions();

	QHBoxLayout* Buttons = new QHBoxLayout();
	QPushButton* SaveCurrentButton = new QPushButton("Save Current");
	QPushButton* ImportButton = new QPushButton("Import workshop.json");
	QPushButton* ActivateButton = new QPushButton("Select Version");
	QPushButton* EditActiveButton = new QPushButton("Edit Active JSON");
	QPushButton* EditSelectedButton = new QPushButton("Edit Selected JSON");
	QPushButton* DeleteButton = new QPushButton("Delete Selected");
	QPushButton* OpenFolderButton = new QPushButton("Open Folder");
	Buttons->addWidget(SaveCurrentButton);
	Buttons->addWidget(ImportButton);
	Buttons->addWidget(ActivateButton);
	Buttons->addWidget(EditActiveButton);
	Buttons->addWidget(EditSelectedButton);
	Buttons->addWidget(DeleteButton);
	Buttons->addWidget(OpenFolderButton);
	Layout->addLayout(Buttons);

	connect(SaveCurrentButton, &QPushButton::clicked, &Dialog, [&, this]()
	{
		const QString CurrentJson = QDir::cleanPath(QString("%1/workshop.json").arg(ActiveFolder));
		if (!QFileInfo(CurrentJson).isFile())
		{
			QMessageBox::information(&Dialog, "Workshop Versions", "No active workshop.json was found in the live zone folder.");
			return;
		}

		bool Accepted = false;
		const QString Label = QInputDialog::getText(&Dialog, "Save Current Version", "Version label:", QLineEdit::Normal, "", &Accepted).trimmed();
		if (!Accepted)
			return;

		if (SaveWorkshopVersionSnapshot(ItemType, ContainerName, EntryName, CurrentJson, Label))
			RefreshVersions();
	});

	connect(ImportButton, &QPushButton::clicked, &Dialog, [&, this]()
	{
		const QString ImportPath = QFileDialog::getOpenFileName(&Dialog, "Import workshop.json", QString(), "workshop.json (workshop.json);;JSON Files (*.json)");
		if (ImportPath.isEmpty())
			return;

		bool Accepted = false;
		const QString Label = QInputDialog::getText(&Dialog, "Import Workshop Version", "Version label:", QLineEdit::Normal, QFileInfo(ImportPath).baseName(), &Accepted).trimmed();
		if (!Accepted)
			return;

		const QString OverrideId = QInputDialog::getText(&Dialog, "Workshop ID", "Optional PublisherID override:", QLineEdit::Normal).trimmed();
		if (SaveWorkshopVersionSnapshot(ItemType, ContainerName, EntryName, ImportPath, Label, OverrideId))
			RefreshVersions();
	});

	connect(ActivateButton, &QPushButton::clicked, &Dialog, [&, this]()
	{
		QListWidgetItem* Current = VersionsList->currentItem();
		if (!Current)
			return;

		if (ActivateWorkshopVersion(ItemType, ContainerName, EntryName, Current->data(Qt::UserRole).toString()))
		{
			RefreshVersions();
			Dialog.accept();
		}
		else
			QMessageBox::warning(&Dialog, "Workshop Versions", "Unable to activate the selected version.");
	});
	connect(EditActiveButton, &QPushButton::clicked, &Dialog, [&]()
	{
		const QString ActiveJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(ActiveFolder));
		if (EditJsonTextDialog(&Dialog, ActiveJsonPath, "Edit Active workshop.json"))
			RefreshVersions();
	});
	connect(EditSelectedButton, &QPushButton::clicked, &Dialog, [&]()
	{
		QListWidgetItem* Current = VersionsList->currentItem();
		if (!Current)
			return;
		const QString SelectedJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(Current->data(Qt::UserRole).toString()));
		if (EditJsonTextDialog(&Dialog, SelectedJsonPath, "Edit Saved workshop.json"))
			RefreshVersions();
	});
	connect(VersionsList, &QListWidget::itemDoubleClicked, &Dialog, [&, this](QListWidgetItem* Current)
	{
		if (Current && ActivateWorkshopVersion(ItemType, ContainerName, EntryName, Current->data(Qt::UserRole).toString()))
		{
			RefreshVersions();
			Dialog.accept();
		}
	});

	connect(DeleteButton, &QPushButton::clicked, &Dialog, [&, this]()
	{
		QListWidgetItem* Current = VersionsList->currentItem();
		if (!Current)
			return;

		const QString FolderPath = Current->data(Qt::UserRole).toString();
		if (QMessageBox::warning(&Dialog, "Delete Workshop Version", QString("Delete workshop version '%1'?").arg(QFileInfo(FolderPath).fileName()), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		QDir(FolderPath).removeRecursively();
		RefreshVersions();
	});

	connect(OpenFolderButton, &QPushButton::clicked, &Dialog, [&]()
	{
		ShellExecute(NULL, "open", QString("\"%1\"").arg(QDir::toNativeSeparators(VersionsRoot)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	});

	QDialogButtonBox* CloseButtons = new QDialogButtonBox(QDialogButtonBox::Close, &Dialog);
	connect(CloseButtons, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	Layout->addWidget(CloseButtons);
	Dialog.exec();
}

void mlMainWindow::UpdateDB()
{
	if (mBuildThread)
		return;

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));

	StartBuildThread(Commands);
}

void mlMainWindow::StartBuildThread(const QList<QPair<QString, QStringList>>& Commands)
{
	mOutputWidget->clear();
	if (mOutputPlainWidget)
		mOutputPlainWidget->clear();
	mPendingOutput.clear();
	mOutputFullText.clear();
	mCurrentOutputBlockItem = NULL;
	mOutputBlockCounter = 0;
	mOutputTreeAutoFollow = true;
	mOutputPlainAutoFollow = true;
	for (const QPair<QString, QStringList>& Command : Commands)
	{
		if (IsGameLaunchCommand(Command.first, Command.second) && ShouldUseSteamOnlineLaunch())
		{
			QString TargetLabel = "selected content";
			const int FsGameIdx = Command.second.indexOf("fs_game");
			if (FsGameIdx >= 0 && FsGameIdx + 1 < Command.second.count())
				TargetLabel = QString("mod '%1'").arg(Command.second[FsGameIdx + 1]);
			const int DevmapIdx = Command.second.indexOf("+devmap");
			if (DevmapIdx >= 0 && DevmapIdx + 1 < Command.second.count())
				TargetLabel = QString("map '%1'").arg(Command.second[DevmapIdx + 1]);
			ShowOnlineLaunchProgressDialog(TargetLabel);
			break;
		}
	}
	for (const QPair<QString, QStringList>& Command : Commands)
	{
		if (IsGameLaunchCommand(Command.first, Command.second))
		{
			mPendingLaunchRequestedMs = mStatsElapsedTimer.isValid() ? mStatsElapsedTimer.elapsed() : 1;
			UpdateGameRunningState();
			break;
		}
	}

	mBuildThread = new mlBuildThread(Commands, mIgnoreErrorsWidget->isChecked());
	connect(mBuildThread, SIGNAL(OutputReady(QString)), this, SLOT(BuildOutputReady(QString)));
	connect(mBuildThread, &mlBuildThread::CommandFinished, this, &mlMainWindow::OnBuildCommandFinished);
	connect(mBuildThread, SIGNAL(finished()), this, SLOT(BuildFinished()));
	mBuildThread->start();
}

void mlMainWindow::StartConvertThread(QStringList& pathList, QString& outputDir, bool allowOverwrite)
{
	if (mOutputWidget)
		mOutputWidget->clear();
	mCurrentOutputBlockItem = NULL;
	mOutputBlockCounter = 0;
	if (mOutputPlainWidget)
		mOutputPlainWidget->clear();
	mPendingOutput.clear();
	mOutputFullText.clear();
	mOutputTreeAutoFollow = true;
	mOutputPlainAutoFollow = true;
	mConvertThread = new mlConvertThread(pathList, outputDir, true, allowOverwrite);
	connect(mConvertThread, SIGNAL(OutputReady(QString)), this, SLOT(BuildOutputReady(QString)));
	connect(mConvertThread, SIGNAL(finished()), this, SLOT(BuildFinished()));
	mConvertThread->start();
}

void mlMainWindow::SetActiveBuildButton(BuildLanguageMode Mode)
{
	ResetBuildButtons();
	mActiveBuildButton = (Mode == BuildAllLanguages) ? mBuildButton : mBuildAllLanguagesButton;
	mActiveBuildButton->setText("Cancel");
	((mActiveBuildButton == mBuildButton) ? mBuildAllLanguagesButton : mBuildButton)->setEnabled(false);
}

void mlMainWindow::ResetBuildButtons()
{
	mActiveBuildButton = NULL;
	UpdateBuildActionButtons();
}

void mlMainWindow::UpdateBuildActionButtons()
{
	if (!mBuildButton || !mBuildAllLanguagesButton)
		return;

	if (mBuildThread)
		return;

	const bool RunOnlyMode = mRunEnabledWidget && mRunEnabledWidget->isChecked()
		&& mCompileEnabledWidget && !mCompileEnabledWidget->isChecked()
		&& mLightEnabledWidget && !mLightEnabledWidget->isChecked()
		&& mLinkEnabledWidget && !mLinkEnabledWidget->isChecked();
	const bool HasCheckedTargets = !CollectTargetItems().isEmpty();
	QTreeWidgetItem* CurrentItem = mFileListWidget ? mFileListWidget->currentItem() : NULL;
	const bool HasValidSelection = CurrentItem && CurrentItem->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN;
	const bool HasRunnableSelection = HasValidSelection
		&& CurrentItem->data(0, ML_ITEM_CHECKSTATE_ROLE).toInt() == Qt::Checked;
	const bool HasBuildTarget = HasCheckedTargets || HasValidSelection;
	const bool HasEnabledAction = (mCompileEnabledWidget && mCompileEnabledWidget->isChecked())
		|| (mLightEnabledWidget && mLightEnabledWidget->isChecked())
		|| (mLinkEnabledWidget && mLinkEnabledWidget->isChecked())
		|| (mRunEnabledWidget && mRunEnabledWidget->isChecked());
	const bool CanBuild = HasBuildTarget;
	const bool CanRunSelectedOnly = RunOnlyMode && HasRunnableSelection;
	const bool CanAnalyzeCheckedTarget = HasCheckedTargets;

	QString RunOnlyLabel = "Start Game";
	QString RunOnlyPrimaryLabel = "Start Game";
	if (HasRunnableSelection)
	{
		const int ItemType = CurrentItem->data(0, Qt::UserRole).toInt();
		RunOnlyLabel = (ItemType == ML_ITEM_MAP) ? "Start Map" : "Start Mod";
		RunOnlyPrimaryLabel = RunOnlyLabel;
	}
	QString BuildLanguageLabel = mBuildLanguage.trimmed().isEmpty() || mBuildLanguage.compare("All", Qt::CaseInsensitive) == 0
		? QString("English")
		: mBuildLanguage.trimmed();
	if (!BuildLanguageLabel.isEmpty())
		BuildLanguageLabel[0] = BuildLanguageLabel[0].toUpper();

	mBuildAllLanguagesButton->setText(RunOnlyMode ? RunOnlyPrimaryLabel : QString("Build (%1)").arg(BuildLanguageLabel));
	mBuildAllLanguagesButton->setEnabled(CanBuild || CanRunSelectedOnly);
	mBuildButton->setText(RunOnlyMode ? RunOnlyLabel : "Build (All)");
	mBuildButton->setEnabled(RunOnlyMode ? CanRunSelectedOnly : CanBuild);
	if (mActionEditBuild)
		mActionEditBuild->setText(RunOnlyMode ? RunOnlyLabel : "Build (All)");
	if (mActionEditBuild)
		mActionEditBuild->setEnabled(CanBuild || CanRunSelectedOnly);
	if (mActionEditBuildAllLanguages)
		mActionEditBuildAllLanguages->setText(RunOnlyMode ? RunOnlyPrimaryLabel : QString("Build (%1)").arg(BuildLanguageLabel));
	if (mActionEditBuildAllLanguages)
		mActionEditBuildAllLanguages->setEnabled(RunOnlyMode ? CanRunSelectedOnly : CanBuild);
	if (mActionEditInformation)
		mActionEditInformation->setEnabled(HasCheckedTargets);
	if (mAnalyzeItemButton)
		mAnalyzeItemButton->setEnabled(CanAnalyzeCheckedTarget);
	if (mActionEditAnalyze)
		mActionEditAnalyze->setEnabled(CanAnalyzeCheckedTarget);
	if (mActionEditReadyForPublish)
		mActionEditReadyForPublish->setEnabled(HasCheckedTargets);
	if (mActionEditPublish)
		mActionEditPublish->setEnabled(HasCheckedTargets);
}

void mlMainWindow::ApplyLauncherLayout()
{
	if (!mTopWidget || !mTopLayout || !mActionsPanel || !mAssetDockWidget || !mOutputDockWidget)
		return;

	while (mTopLayout->count() > 0)
	{
		QLayoutItem* Item = mTopLayout->takeAt(0);
		delete Item;
	}

	mTopLayout->addStretch(1);
	mTopLayout->addWidget(mActionsPanel);
	mTopLayout->addStretch(1);
	mTopWidget->show();
	mActionsPanel->show();
	mAssetDockWidget->setFloating(false);
	mOutputDockWidget->setFloating(false);
	removeDockWidget(mAssetDockWidget);
	removeDockWidget(mOutputDockWidget);
	addDockWidget(Qt::LeftDockWidgetArea, mAssetDockWidget);
	mAssetDockWidget->show();
	mOutputDockWidget->show();

	const QString LayoutKey = mLauncherLayout.trimmed().toLower();
	if (LayoutKey == "left-build-console")
	{
		addDockWidget(Qt::RightDockWidgetArea, mOutputDockWidget);
		resizeDocks(QList<QDockWidget*>() << mAssetDockWidget << mOutputDockWidget, QList<int>() << 430 << 520, Qt::Horizontal);
	}
	else if (LayoutKey == "left-console-build")
	{
		addDockWidget(Qt::LeftDockWidgetArea, mOutputDockWidget);
		splitDockWidget(mAssetDockWidget, mOutputDockWidget, Qt::Horizontal);
		resizeDocks(QList<QDockWidget*>() << mAssetDockWidget << mOutputDockWidget, QList<int>() << 430 << 520, Qt::Horizontal);
	}
	else
	{
		addDockWidget(Qt::BottomDockWidgetArea, mOutputDockWidget);
		resizeDocks(QList<QDockWidget*>() << mAssetDockWidget, QList<int>() << 430, Qt::Horizontal);
		resizeDocks(QList<QDockWidget*>() << mOutputDockWidget, QList<int>() << 360, Qt::Vertical);
	}
	mAssetDockWidget->raise();
	mOutputDockWidget->raise();
}

QTreeWidgetItem* mlMainWindow::ActiveFileItem() const
{
	if (!mFileListWidget)
		return NULL;

	QTreeWidgetItem* Item = mFileListWidget->currentItem();
	if (!Item)
	{
		const QList<QTreeWidgetItem*> SelectedItems = mFileListWidget->selectedItems();
		if (!SelectedItems.isEmpty())
			Item = SelectedItems[0];
	}

	if (!Item || Item->data(0, Qt::UserRole).toInt() == ML_ITEM_UNKNOWN)
		return NULL;

	return Item;
}

QString mlMainWindow::GetItemContainerName(QTreeWidgetItem* Item) const
{
	return Item ? Item->data(0, ML_ITEM_CONTAINER_ROLE).toString() : QString();
}

QString mlMainWindow::GetItemEntryName(QTreeWidgetItem* Item) const
{
	return Item ? Item->data(0, ML_ITEM_NAME_ROLE).toString() : QString();
}

QString mlMainWindow::GetItemFavoriteKey(QTreeWidgetItem* Item) const
{
	return Item ? Item->data(0, ML_ITEM_FAVORITE_ROLE).toString() : QString();
}

bool mlMainWindow::IsCustomTab(int TabIndex) const
{
	if (!mCategoryTabs || TabIndex < 0 || TabIndex >= mCategoryTabs->count())
		return false;
	return mCategoryTabs->tabData(TabIndex).toString().startsWith("custom-");
}

void mlMainWindow::RebuildCategoryTabs()
{
	if (!mCategoryTabs)
		return;

	const QString SavedKey = CurrentCategoryTabKey(mCategoryTabs);

	// Block signals so currentChanged doesn't fire during rebuild
	mCategoryTabs->blockSignals(true);
	while (mCategoryTabs->count() > 0)
		mCategoryTabs->removeTab(0);

	QSettings Settings;
	auto AddTab = [&](const QString& DefaultLabel, const QString& Key)
	{
		const QString OverrideName = Settings.value(QString("TabOverrides/%1/Name").arg(Key)).toString();
		const QString OverrideColor = Settings.value(QString("TabOverrides/%1/Color").arg(Key)).toString();
		const QString OverrideIcon = Settings.value(QString("TabOverrides/%1/Icon").arg(Key)).toString();
		const QString ActualLabel = BuildTabLabel(OverrideIcon, OverrideName.isEmpty() ? DefaultLabel : OverrideName);
		const int Idx = mCategoryTabs->addTab(ActualLabel);
		mCategoryTabs->setTabIcon(Idx, BuildTabIcon(OverrideIcon));
		mCategoryTabs->setTabData(Idx, Key);
		if (!OverrideColor.isEmpty())
		{
			const QColor Col(OverrideColor);
			if (Col.isValid())
				mCategoryTabs->setTabTextColor(Idx, Col);
		}
	};
	AddTab("All", "all");
	AddTab("Recent", "recent");
	AddTab("Favorites", "favorites");
	AddTab("ZM Maps", "zm-maps");
	AddTab("MP Maps", "mp-maps");
	AddTab("Mods", "mods");

	const QList<QVariantMap> CustomTabDefs = LoadCustomTabDefs();
	for (const QVariantMap& Tab : CustomTabDefs)
	{
		const QString TabId = Tab.value("id").toString();
		const QString TabName = Tab.value("name").toString();
		const QString TabIconValue = Tab.value("icon").toString();
		const int Idx = mCategoryTabs->addTab(BuildTabLabel(TabIconValue, TabName));
		mCategoryTabs->setTabIcon(Idx, BuildTabIcon(TabIconValue));
		mCategoryTabs->setTabData(Idx, TabId);
		if (!Tab.value("color").toString().isEmpty())
		{
			const QColor Col(Tab.value("color").toString());
			if (Col.isValid())
				mCategoryTabs->setTabTextColor(Idx, Col);
		}
	}
	const int AddTabIdx = mCategoryTabs->addTab("+");
	mCategoryTabs->setTabData(AddTabIdx, "__add-category-tab__");
	mCategoryTabs->setTabToolTip(AddTabIdx, "Add custom tab");

	mCategoryTabs->blockSignals(false);

	// Restore previously active tab
	const int RestoredIdx = FindCategoryTabIndex(mCategoryTabs, SavedKey);
	mCategoryTabs->setCurrentIndex((RestoredIdx >= 0 && !IsAddCategoryTabKey(mCategoryTabs->tabData(RestoredIdx).toString())) ? RestoredIdx : 0);
	PopulateFileList();
}

void mlMainWindow::ShowCategoryTabEditDialog(int TabIndex)
{
	if (!mCategoryTabs)
		return;

	const bool IsNew = (TabIndex < 0) || (TabIndex >= 0 && IsAddCategoryTabKey(mCategoryTabs->tabData(TabIndex).toString()));
	const bool IsCustom = IsNew || IsCustomTab(TabIndex);

	// Resolve current values
	QString CurrentId;
	QString CurrentName;
	QString CurrentIcon;
	QString CurrentColor;
	QStringList CurrentItems;

	if (!IsNew)
	{
		CurrentId = mCategoryTabs->tabData(TabIndex).toString();
		if (IsCustom)
		{
			// Extract from saved custom tab def
			const QList<QVariantMap> Defs = LoadCustomTabDefs();
			for (const QVariantMap& Tab : Defs)
			{
				if (Tab.value("id").toString() == CurrentId)
				{
					CurrentName = Tab.value("name").toString();
					CurrentIcon = Tab.value("icon").toString();
					CurrentColor = Tab.value("color").toString();
					CurrentItems = Tab.value("items").toStringList();
					break;
				}
			}
		}
		else
		{
			// Built-in tab: read name from settings override or current tab text
			QSettings Settings;
			CurrentName = Settings.value(QString("TabOverrides/%1/Name").arg(CurrentId)).toString();
			CurrentIcon = Settings.value(QString("TabOverrides/%1/Icon").arg(CurrentId)).toString();
			CurrentColor = Settings.value(QString("TabOverrides/%1/Color").arg(CurrentId)).toString();
			if (CurrentName.isEmpty())
			{
				// Strip any icon prefix from the current tab label to get the base name
				QString TabText = mCategoryTabs->tabText(TabIndex);
				if (TabText.contains("  "))
					TabText = TabText.mid(TabText.indexOf("  ") + 2);
				CurrentName = TabText.trimmed();
			}
		}
	}

	// Build dialog
	QDialog Dialog(this);
	Dialog.setWindowTitle(IsNew ? "New Custom Tab" : (IsCustom ? "Edit Custom Tab" : "Edit Tab"));
	Dialog.resize(700, IsCustom ? 560 : 380);
	Dialog.setMinimumSize(620, IsCustom ? 500 : 340);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	Layout->setSpacing(8);

	// Name
	Layout->addWidget(new QLabel("Tab Name:", &Dialog));
	QLineEdit* NameEdit = new QLineEdit(CurrentName, &Dialog);
	NameEdit->setPlaceholderText("e.g. My Maps");
	Layout->addWidget(NameEdit);

	// Icon
	Layout->addWidget(new QLabel("Icon image:", &Dialog));
	QWidget* IconRow = new QWidget(&Dialog);
	QHBoxLayout* IconRowLayout = new QHBoxLayout(IconRow);
	IconRowLayout->setContentsMargins(0, 0, 0, 0);
	IconRowLayout->setSpacing(6);
	BackgroundDropLineEdit* IconEdit = new BackgroundDropLineEdit(CurrentIcon, IconRow);
	IconEdit->setPlaceholderText("Drop an icon file here or browse...");
	QLabel* IconPreview = new QLabel("None", IconRow);
	IconPreview->setObjectName("BackgroundPreview");
	IconPreview->setFixedSize(28, 28);
	IconPreview->setAlignment(Qt::AlignCenter);
	IconPreview->setToolTip("Current tab icon preview");
	QPushButton* IconBrowseBtn = new QPushButton("Browse...", IconRow);
	QPushButton* IconClearBtn = new QPushButton("Clear", IconRow);
	IconRowLayout->addWidget(IconEdit, 1);
	IconRowLayout->addWidget(IconPreview);
	IconRowLayout->addWidget(IconBrowseBtn);
	IconRowLayout->addWidget(IconClearBtn);
	Layout->addWidget(IconRow);
	auto RefreshIconPreview = [=]()
	{
		UpdateBackgroundPreviewLabel(IconPreview, IconEdit->text());
	};
	connect(IconBrowseBtn, &QPushButton::clicked, &Dialog, [&, IconEdit]()
	{
		const QString FileName = QFileDialog::getOpenFileName(&Dialog, "Select Icon", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp *.ico);;All Files (*)");
		if (!FileName.isEmpty())
			IconEdit->setText(QDir::toNativeSeparators(FileName));
	});
	connect(IconClearBtn, &QPushButton::clicked, &Dialog, [=]() { IconEdit->clear(); });
	connect(IconEdit, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshIconPreview(); });
	RefreshIconPreview();

	// Color
	Layout->addWidget(new QLabel("Tab name color:", &Dialog));
	QWidget* ColorWidget = new QWidget(&Dialog);
	QHBoxLayout* ColorRow = new QHBoxLayout(ColorWidget);
	ColorRow->setContentsMargins(0, 0, 0, 0);
	ColorRow->setSpacing(6);
	QLineEdit* ColorEdit = new QLineEdit(CurrentColor, ColorWidget);
	ColorEdit->setPlaceholderText("#ff8a2a");
	QPushButton* ColorBrowseButton = new QPushButton(ColorWidget);
	ColorBrowseButton->setFixedSize(26, 26);
	ColorBrowseButton->setToolTip("Pick tab name color");
	QPushButton* ColorClearBtn = new QPushButton("Clear", ColorWidget);
	ColorRow->addWidget(ColorEdit, 1);
	ColorRow->addWidget(ColorBrowseButton);
	ColorRow->addWidget(ColorClearBtn);
	Layout->addWidget(ColorWidget);
	auto RefreshColorButton = [=]()
	{
		const QString NormalizedColor = NormalizedStoredColor(ColorEdit->text());
		const QString SafeColor = NormalizedColor.isEmpty() ? QString("#444444") : NormalizedColor;
		ColorBrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(SafeColor));
	};
	connect(ColorBrowseButton, &QPushButton::clicked, &Dialog, [&, ColorEdit]()
	{
		QColor Color = QColorDialog::getColor(QColor(ColorEdit->text()), &Dialog, "Select Tab Name Color");
		if (Color.isValid())
			ColorEdit->setText(Color.name(QColor::HexRgb));
	});
	connect(ColorEdit, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshColorButton(); });
	connect(ColorClearBtn, &QPushButton::clicked, &Dialog, [=]() { ColorEdit->clear(); });
	RefreshColorButton();

	// Items list (custom tabs only)
	QListWidget* ItemsList = nullptr;
	if (IsCustom)
	{
		Layout->addWidget(new QLabel("Maps and Mods in this tab:", &Dialog));
		ItemsList = new QListWidget(&Dialog);
		ItemsList->setSelectionMode(QAbstractItemView::NoSelection);
		ItemsList->setMinimumHeight(140);

		// Add all maps
		const QString UserMapsFolder = QDir::cleanPath(QString("%1/usermaps/").arg(mGamePath));
		const QStringList UserMaps = QDir(UserMapsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
		for (const QString& MapName : UserMaps)
		{
			const QString ZoneFile = QString("%1/%2/zone_source/%3.zone").arg(UserMapsFolder, MapName, MapName);
			if (!QFileInfo(ZoneFile).isFile())
				continue;
			const QString FavKey = QString("map:%1").arg(MapName.toLower());
			QListWidgetItem* LItem = new QListWidgetItem(MapName, ItemsList);
			LItem->setData(Qt::UserRole, FavKey);
			LItem->setCheckState(CurrentItems.contains(FavKey, Qt::CaseInsensitive) ? Qt::Checked : Qt::Unchecked);
		}

		// Add all mods
		const QString ModsFolder = QDir::cleanPath(QString("%1/mods/").arg(mGamePath));
		const QStringList Mods = QDir(ModsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
		const char* ZoneFiles[4] = { "core_mod", "mp_mod", "cp_mod", "zm_mod" };
		for (const QString& ModName : Mods)
		{
			bool HasZone = false;
			for (int f = 0; f < 4; f++)
			{
				if (QFileInfo(QString("%1/%2/zone_source/%3.zone").arg(ModsFolder, ModName, QString(ZoneFiles[f]))).isFile())
				{
					HasZone = true;
					break;
				}
			}
			if (!HasZone)
				continue;
			const QString FavKey = QString("mod:%1").arg(ModName.toLower());
			QListWidgetItem* LItem = new QListWidgetItem(QString("Mod: %1").arg(ModName), ItemsList);
			LItem->setData(Qt::UserRole, FavKey);
			LItem->setCheckState(CurrentItems.contains(FavKey, Qt::CaseInsensitive) ? Qt::Checked : Qt::Unchecked);
		}

		if (ItemsList->count() == 0)
		{
			QListWidgetItem* Empty = new QListWidgetItem("(No maps or mods found)", ItemsList);
			Empty->setFlags(Qt::NoItemFlags);
		}

		Layout->addWidget(ItemsList);
	}

	// Buttons
	QDialogButtonBox* Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	connect(Buttons, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
	connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
	Layout->addWidget(Buttons);

	const int Result = Dialog.exec();

	if (Result != QDialog::Accepted)
		return;

	const QString NewName = NameEdit->text().trimmed();
	if (NewName.isEmpty())
		return;

	const QString NewIcon = IconEdit->text().trimmed();
	const QString PickedColor = NormalizedStoredColor(ColorEdit->text());

	// Collect checked items for custom tabs
	QStringList NewItems;
	if (ItemsList)
	{
		for (int i = 0; i < ItemsList->count(); i++)
		{
			QListWidgetItem* LItem = ItemsList->item(i);
			if (LItem && (LItem->flags() & Qt::ItemIsEnabled) && LItem->checkState() == Qt::Checked)
				NewItems.append(LItem->data(Qt::UserRole).toString());
		}
	}

	if (IsNew)
	{
		// Create new custom tab
		QVariantMap NewTab;
		NewTab["id"] = GenerateCustomTabId();
		NewTab["name"] = NewName;
		NewTab["icon"] = NewIcon;
		NewTab["color"] = PickedColor;
		NewTab["items"] = NewItems;
		QList<QVariantMap> Defs = LoadCustomTabDefs();
		Defs.append(NewTab);
		SaveCustomTabDefs(Defs);
		const QString NewId = NewTab["id"].toString();
		RebuildCategoryTabs();
		const int NewIdx = FindCategoryTabIndex(mCategoryTabs, NewId);
		if (NewIdx >= 0)
			mCategoryTabs->setCurrentIndex(NewIdx);
	}
	else if (IsCustom)
	{
		// Update existing custom tab
		QList<QVariantMap> Defs = LoadCustomTabDefs();
		for (auto& Tab : Defs)
		{
			if (Tab.value("id").toString() == CurrentId)
			{
				Tab["name"] = NewName;
				Tab["icon"] = NewIcon;
				Tab["color"] = PickedColor;
				Tab["items"] = NewItems;
				break;
			}
		}
		SaveCustomTabDefs(Defs);
		RebuildCategoryTabs();
	}
	else
	{
		// Update built-in tab override
		QSettings Settings;
		Settings.setValue(QString("TabOverrides/%1/Name").arg(CurrentId), NewName);
		Settings.setValue(QString("TabOverrides/%1/Icon").arg(CurrentId), NewIcon);
		Settings.setValue(QString("TabOverrides/%1/Color").arg(CurrentId), PickedColor);
		RebuildCategoryTabs();
	}
}

QStringList mlMainWindow::FavoriteEntries() const
{
	QSettings Settings;
	return Settings.value("Favorites", QStringList()).toStringList();
}

QStringList mlMainWindow::RecentEntries() const
{
	QSettings Settings;
	return Settings.value("Recents", QStringList()).toStringList();
}

bool mlMainWindow::IsFavoriteEntry(const QString& Entry) const
{
	return FavoriteEntries().contains(Entry, Qt::CaseInsensitive);
}

void mlMainWindow::ToggleFavoriteEntry(const QString& Entry)
{
	if (Entry.isEmpty())
		return;

	QSettings Settings;
	QStringList Entries = FavoriteEntries();
	auto ToggleSingleEntry = [&](const QString& Value)
	{
		const int Index = Entries.indexOf(QRegularExpression(QString("^%1$").arg(QRegularExpression::escape(Value)), QRegularExpression::CaseInsensitiveOption));

		if (Index >= 0)
			Entries.removeAt(Index);
		else
			Entries.append(Value);
	};

	ToggleSingleEntry(Entry);
	QRegularExpression ModGroupExpression("^mod:([^/]+)$", QRegularExpression::CaseInsensitiveOption);
	QRegularExpressionMatch Match = ModGroupExpression.match(Entry);
	if (Match.hasMatch())
	{
		for (const QString& ZoneName : ModZoneNames(Match.captured(1)))
			ToggleSingleEntry(QString("mod:%1/%2").arg(Match.captured(1).toLower(), ZoneName.toLower()));
	}

	Settings.setValue("Favorites", Entries);
}

void mlMainWindow::TouchRecentEntry(const QString& Entry)
{
	if (Entry.isEmpty())
		return;

	QSettings Settings;
	QStringList Entries = RecentEntries();
	const int Index = Entries.indexOf(QRegularExpression(QString("^%1$").arg(QRegularExpression::escape(Entry)), QRegularExpression::CaseInsensitiveOption));
	if (Index >= 0)
		Entries.removeAt(Index);
	Entries.prepend(Entry);
	while (Entries.count() > 12)
		Entries.removeLast();
	Settings.setValue("Recents", Entries);
}

QString mlMainWindow::RecentEntryForItem(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	if (ItemType == ML_ITEM_MAP)
		return QString("map:%1").arg(EntryName.toLower());

	if (ItemType == ML_ITEM_MOD || ItemType == ML_ITEM_MOD_GROUP)
		return QString("mod:%1").arg(ContainerName.toLower());

	return QString();
}

QStringList mlMainWindow::ModZoneNames(const QString& ModName) const
{
	QStringList Zones;
	const char* Files[4] = { "core_mod", "mp_mod", "cp_mod", "zm_mod" };
	for (int FileIdx = 0; FileIdx < 4; FileIdx++)
	{
		QString ZoneFileName = QString("%1/mods/%2/zone_source/%3.zone").arg(mGamePath, ModName, Files[FileIdx]);
		if (QFileInfo(ZoneFileName).isFile())
			Zones << Files[FileIdx];
	}
	return Zones;
}

Qt::CheckState mlMainWindow::CheckStateForKey(const QString& FavoriteKey) const
{
	if (FavoriteKey.isEmpty())
		return Qt::Unchecked;

	return mCheckedStateByKey.value(FavoriteKey.toLower(), Qt::Unchecked);
}

static Qt::CheckState ItemCheckState(QTreeWidgetItem* Item)
{
	return Item ? static_cast<Qt::CheckState>(Item->data(0, ML_ITEM_CHECKSTATE_ROLE).toInt()) : Qt::Unchecked;
}

void mlMainWindow::ApplyCheckStateForKey(const QString& FavoriteKey, Qt::CheckState State, QTreeWidgetItem* SkipItem)
{
	if (FavoriteKey.isEmpty())
		return;

	const QString NormalizedKey = FavoriteKey.toLower();
	mCheckedStateByKey.insert(NormalizedKey, State);

	std::function<void(QTreeWidgetItem*)> ApplyState = [&](QTreeWidgetItem* Parent)
	{
		for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = Parent->child(ChildIdx);
			if (Child != SkipItem && GetItemFavoriteKey(Child).compare(FavoriteKey, Qt::CaseInsensitive) == 0)
			{
				Child->setData(0, ML_ITEM_CHECKSTATE_ROLE, State);
				SyncItemCheckWidget(Child);
			}

			ApplyState(Child);
		}
	};

	ApplyState(mFileListWidget->invisibleRootItem());
}

void mlMainWindow::SyncItemCheckWidget(QTreeWidgetItem* Item) const
{
	if (!Item)
		return;

	QWidget* Widget = mFileListWidget ? mFileListWidget->itemWidget(Item, 0) : NULL;
	if (!Widget)
		return;

	QCheckBox* CheckBox = Widget->findChild<QCheckBox*>("ItemSelectCheckBox");
	if (!CheckBox)
		return;

	CheckBox->blockSignals(true);
	CheckBox->setChecked(ItemCheckState(Item) == Qt::Checked);
	CheckBox->blockSignals(false);
}

void mlMainWindow::UpdateParentCheckState(QTreeWidgetItem* Item)
{
	QTreeWidgetItem* Parent = Item ? Item->parent() : NULL;
	while (Parent)
	{
		int CheckedChildren = 0;
		for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
		{
			if (ItemCheckState(Parent->child(ChildIdx)) == Qt::Checked)
				CheckedChildren++;
		}

		const Qt::CheckState NewState = (CheckedChildren == Parent->childCount() && Parent->childCount() > 0) ? Qt::Checked : Qt::Unchecked;
		const QString ParentKey = GetItemFavoriteKey(Parent);
		if (!ParentKey.isEmpty())
			ApplyCheckStateForKey(ParentKey, NewState);
		else
		{
			Parent->setData(0, ML_ITEM_CHECKSTATE_ROLE, NewState);
			SyncItemCheckWidget(Parent);
		}
		Parent = Parent->parent();
	}
}

void mlMainWindow::RefreshRunDvars()
{
	mRunDvars.clear();
	for (int DvarIdx = 0; DvarIdx < ARRAYSIZE(gDvars); DvarIdx++)
	{
		const dvar_s DvarEntry = gDvars[DvarIdx];
		const QString DvarValue = Dvar::launchValue(DvarEntry);
		if (DvarValue.isEmpty())
			continue;

		if (!DvarEntry.isCmd)
			mRunDvars << "+set" << DvarEntry.name;
		else
			mRunDvars << QString("+%1").arg(DvarEntry.name);
		mRunDvars << DvarValue;
	}
}

bool mlMainWindow::IsNewerLauncherVersion(const QString& AvailableVersion) const
{
	const QVersionNumber CurrentVersion = VersionNumberFromText(QString::fromLatin1(kLauncherVersion));
	const QVersionNumber LatestVersion = VersionNumberFromText(AvailableVersion);
	return !LatestVersion.isNull() && QVersionNumber::compare(LatestVersion, CurrentVersion) > 0;
}

QString mlMainWindow::UpdateApiUrl() const
{
	return QString::fromLatin1(kLauncherReleaseApiUrl).trimmed();
}

QString mlMainWindow::UpdateReleasesPageUrl() const
{
	return QString::fromLatin1(kLauncherReleasesUrl).trimmed();
}

QJsonObject mlMainWindow::SelectPrimaryReleaseAsset(const QJsonArray& Assets) const
{
	QJsonObject FallbackAsset;
	for (const QJsonValue& AssetValue : Assets)
	{
		const QJsonObject Asset = AssetValue.toObject();
		const QString AssetName = Asset.value("name").toString().trimmed();
		const QString AssetUrl = Asset.value("browser_download_url").toString().trimmed();
		if (AssetName.isEmpty() || AssetUrl.isEmpty())
			continue;

		const QString LowerName = AssetName.toLower();
		if (!LowerName.endsWith(".zip"))
			continue;

		if (FallbackAsset.isEmpty())
			FallbackAsset = Asset;

		if (!LowerName.contains("original"))
			return Asset;
	}
	return FallbackAsset;
}

void mlMainWindow::CheckForUpdates(bool UserInitiated)
{
	if (!mUpdateNetworkAccess)
		return;

	if (mUpdateMetadataReply || mUpdateDownloadReply)
	{
		SetFooterUpdateState("Update status: request already in progress");
		if (UserInitiated)
			QMessageBox::information(this, "Check for Updates", "An update request is already in progress.");
		return;
	}

	const QString ApiUrl = UpdateApiUrl();
	if (ApiUrl.isEmpty())
	{
		SetFooterUpdateState("Update status: no API URL configured");
		if (UserInitiated)
			QMessageBox::information(this, "Check for Updates", "No update API URL is configured yet.");
		return;
	}

	QSettings Settings;
	Settings.setValue("UpdateCheck/LastCheckedUtc", QDateTime::currentDateTimeUtc());
	SetFooterUpdateState("Checking for Update");

	QNetworkRequest Request{ QUrl(ApiUrl) };
	Request.setHeader(QNetworkRequest::UserAgentHeader, QString("BO3-ModLauncher/%1").arg(kLauncherVersion));
	Request.setRawHeader("Accept", "application/vnd.github+json");

	QProgressDialog* ProgressDialog = NULL;
	if (UserInitiated)
	{
		ProgressDialog = new QProgressDialog("Checking for launcher updates...", QString(), 0, 0, this);
		ProgressDialog->setWindowTitle("Check for Updates");
		ProgressDialog->setWindowModality(Qt::WindowModal);
		ProgressDialog->setCancelButton(NULL);
		ProgressDialog->setMinimumDuration(0);
		ProgressDialog->show();
	}

	mUpdateMetadataReply = mUpdateNetworkAccess->get(Request);
	connect(mUpdateMetadataReply, &QNetworkReply::finished, this, [=]()
	{
		if (ProgressDialog)
			ProgressDialog->deleteLater();

		QPointer<QNetworkReply> Reply = mUpdateMetadataReply;
		mUpdateMetadataReply = NULL;
		if (!Reply)
			return;

		const QByteArray ResponseBytes = Reply->readAll();
		const QNetworkReply::NetworkError Error = Reply->error();
		const QString ErrorText = Reply->errorString();
		Reply->deleteLater();

			if (Error != QNetworkReply::NoError)
			{
				SetFooterUpdateState(QString("Update check failed: %1").arg(ErrorText));
				if (UserInitiated)
					QMessageBox::warning(this, "Check for Updates", QString("Unable to check for updates.\n\n%1").arg(ErrorText));
				return;
			}

		QJsonParseError ParseError;
		const QJsonDocument Document = QJsonDocument::fromJson(ResponseBytes, &ParseError);
			if (ParseError.error != QJsonParseError::NoError || !Document.isObject())
			{
				SetFooterUpdateState("Update check failed: invalid JSON");
				if (UserInitiated)
					QMessageBox::warning(this, "Check for Updates", "Update response was not valid JSON.");
				return;
			}

		HandleUpdateMetadata(Document.object(), UserInitiated);
	});
}

void mlMainWindow::HandleUpdateMetadata(const QJsonObject& Root, bool UserInitiated)
{
	const QString LatestVersion = CleanVersionText(Root.value("tag_name").toString().trimmed().isEmpty()
		? Root.value("name").toString().trimmed()
		: Root.value("tag_name").toString().trimmed());

	if (LatestVersion.isEmpty())
	{
		SetFooterUpdateState("Update check failed: version tag missing");
		if (UserInitiated)
			QMessageBox::warning(this, "Check for Updates", "Release metadata did not contain a version tag.");
		return;
	}

	if (!IsNewerLauncherVersion(LatestVersion))
	{
		SetFooterUpdateState("Up to date");
		if (UserInitiated)
			QMessageBox::information(this, "Check for Updates",
				QString("You already have the latest launcher version.\n\nCurrent version: %1").arg(kLauncherVersion));
		return;
	}

	const QJsonObject Asset = SelectPrimaryReleaseAsset(Root.value("assets").toArray());
	const QString DownloadUrl = Asset.value("browser_download_url").toString().trimmed();
	const QString ReleasePageUrl = Root.value("html_url").toString().trimmed().isEmpty() ? UpdateReleasesPageUrl() : Root.value("html_url").toString().trimmed();
	SetFooterUpdateState(QString("New Version available (v%1)").arg(LatestVersion), DownloadUrl, LatestVersion, ReleasePageUrl, true);
	if (statusBar())
		statusBar()->showMessage(QString("Launcher update available: %1").arg(LatestVersion), 10000);

	QSettings Settings;
	if (!UserInitiated && Settings.value("UpdateCheck/DismissedVersion").toString() == LatestVersion)
		return;

	const QString ReleaseBody = Root.value("body").toString().trimmed();
	QString PromptText = QString("A new launcher version is available.\n\nCurrent version: %1\nLatest version: %2")
		.arg(kLauncherVersion, LatestVersion);
	if (!ReleaseBody.isEmpty())
	{
		QString Summary = ReleaseBody;
		if (Summary.count() > 600)
			Summary = Summary.left(600).trimmed() + "...";
		PromptText += QString("\n\nRelease notes:\n%1").arg(Summary);
	}
	PromptText += "\n\nDownload and install it now?";

	QMessageBox Prompt(this);
	Prompt.setIcon(QMessageBox::Information);
	Prompt.setWindowTitle("Launcher Update Available");
	Prompt.setText(PromptText);
	QPushButton* InstallNowButton = Prompt.addButton("Download && Install", QMessageBox::AcceptRole);
	QPushButton* OpenReleasesButton = NULL;
	if (!ReleasePageUrl.isEmpty())
		OpenReleasesButton = Prompt.addButton("Open Releases Page", QMessageBox::ActionRole);
	QPushButton* LaterButton = Prompt.addButton("Later", QMessageBox::RejectRole);
	Prompt.exec();

	if (Prompt.clickedButton() == InstallNowButton)
	{
		if (DownloadUrl.isEmpty())
		{
			QMessageBox::warning(this, "Launcher Update", "No downloadable ZIP asset was found for this release.");
			return;
		}
		Settings.remove("UpdateCheck/DismissedVersion");
		StartUpdateDownload(QUrl(DownloadUrl), LatestVersion);
		return;
	}

	if (Prompt.clickedButton() == OpenReleasesButton)
	{
		QDesktopServices::openUrl(QUrl(ReleasePageUrl));
		return;
	}

	if (Prompt.clickedButton() == LaterButton && !UserInitiated)
		Settings.setValue("UpdateCheck/DismissedVersion", LatestVersion);
}

void mlMainWindow::StartUpdateDownload(const QUrl& DownloadUrl, const QString& VersionLabel)
{
	if (!DownloadUrl.isValid())
	{
		SetFooterUpdateState("Update download failed: invalid URL");
		QMessageBox::warning(this, "Launcher Update", "The release download URL is invalid.");
		return;
	}

	if (mUpdateDownloadReply)
		return;

	SetFooterUpdateState(QString("Downloading %1...").arg(VersionLabel), DownloadUrl.toString(), VersionLabel, mAvailableReleasePageUrl, true);

	const QString TempFolder = QDir::cleanPath(QString("%1/bo3_modlauncher_update_%2")
		.arg(QDir::tempPath(), QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmsszzz")));
	QDir().mkpath(TempFolder);

	mPendingUpdateVersion = VersionLabel;
	mUpdateDownloadPath = QDir::cleanPath(QString("%1/BO3_ModLauncher_%2.zip").arg(TempFolder, VersionLabel));
	if (mUpdateDownloadFile)
	{
		delete mUpdateDownloadFile;
		mUpdateDownloadFile = NULL;
	}
	mUpdateDownloadFile = new QFile(mUpdateDownloadPath, this);
	if (!mUpdateDownloadFile->open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, "Launcher Update", QString("Unable to create the update file:\n%1").arg(mUpdateDownloadPath));
		delete mUpdateDownloadFile;
		mUpdateDownloadFile = NULL;
		return;
	}

	QNetworkRequest Request{ DownloadUrl };
	Request.setHeader(QNetworkRequest::UserAgentHeader, QString("BO3-ModLauncher/%1").arg(kLauncherVersion));
	Request.setRawHeader("Accept", "application/octet-stream");
	mUpdateDownloadReply = mUpdateNetworkAccess->get(Request);

	mUpdateProgressDialog = new QProgressDialog(QString("Downloading launcher update %1...").arg(VersionLabel), "Cancel", 0, 100, this);
	mUpdateProgressDialog->setWindowTitle("Downloading Update");
	mUpdateProgressDialog->setWindowModality(Qt::WindowModal);
	mUpdateProgressDialog->setMinimumDuration(0);
	mUpdateProgressDialog->setValue(0);
	connect(mUpdateProgressDialog, &QProgressDialog::canceled, this, [this]()
	{
		if (mUpdateDownloadReply)
			mUpdateDownloadReply->abort();
	});

	connect(mUpdateDownloadReply, &QNetworkReply::readyRead, this, [this]()
	{
		if (mUpdateDownloadReply && mUpdateDownloadFile)
			mUpdateDownloadFile->write(mUpdateDownloadReply->readAll());
	});
	connect(mUpdateDownloadReply, &QNetworkReply::downloadProgress, this, [this](qint64 Received, qint64 Total)
	{
		if (!mUpdateProgressDialog)
			return;

		if (Total > 0)
		{
			mUpdateProgressDialog->setMaximum(100);
			mUpdateProgressDialog->setValue(qBound(0, static_cast<int>((Received * 100) / Total), 100));
			mUpdateProgressDialog->setLabelText(QString("Downloading launcher update %1...\n%2 / %3 MB")
				.arg(mPendingUpdateVersion)
				.arg(QString::number(Received / 1024.0 / 1024.0, 'f', 2))
				.arg(QString::number(Total / 1024.0 / 1024.0, 'f', 2)));
		}
		else
		{
			mUpdateProgressDialog->setMaximum(0);
			mUpdateProgressDialog->setLabelText(QString("Downloading launcher update %1...").arg(mPendingUpdateVersion));
		}
	});
	connect(mUpdateDownloadReply, &QNetworkReply::finished, this, [this]()
	{
		if (mUpdateDownloadReply && mUpdateDownloadFile)
			mUpdateDownloadFile->write(mUpdateDownloadReply->readAll());

		if (mUpdateDownloadFile)
		{
			mUpdateDownloadFile->flush();
			mUpdateDownloadFile->close();
			delete mUpdateDownloadFile;
			mUpdateDownloadFile = NULL;
		}

		if (mUpdateProgressDialog)
		{
			mUpdateProgressDialog->hide();
			mUpdateProgressDialog->deleteLater();
			mUpdateProgressDialog = NULL;
		}

		QPointer<QNetworkReply> Reply = mUpdateDownloadReply;
		mUpdateDownloadReply = NULL;
		if (!Reply)
			return;

		const QNetworkReply::NetworkError Error = Reply->error();
		const QString ErrorText = Reply->errorString();
		Reply->deleteLater();

			if (Error != QNetworkReply::NoError)
			{
				QFile::remove(mUpdateDownloadPath);
				SetFooterUpdateState(QString("Update download failed: %1").arg(ErrorText));
				QMessageBox::warning(this, "Launcher Update", QString("The update download failed.\n\n%1").arg(ErrorText));
				return;
			}

			if (!LaunchUpdateInstaller(mUpdateDownloadPath, mPendingUpdateVersion))
			{
				QFile::remove(mUpdateDownloadPath);
				SetFooterUpdateState(QString("Update ready: %1").arg(mPendingUpdateVersion), mAvailableUpdateUrl, mPendingUpdateVersion, mAvailableReleasePageUrl, true);
				return;
			}

			SetFooterUpdateState(QString("Installing update %1...").arg(mPendingUpdateVersion), mAvailableUpdateUrl, mPendingUpdateVersion, mAvailableReleasePageUrl, true);
			QMessageBox::information(this, "Launcher Update",
			QString("Launcher update %1 downloaded successfully.\n\nThe launcher will now close so the update can be installed.")
				.arg(mPendingUpdateVersion));
		close();
	});
}

bool mlMainWindow::LaunchUpdateInstaller(const QString& ZipPath, const QString& VersionLabel)
{
	Q_UNUSED(VersionLabel);

	const QString UpdateFolder = QFileInfo(ZipPath).absolutePath();
	const QString ExtractRoot = QDir::cleanPath(QString("%1/extracted").arg(UpdateFolder));
	QDir().mkpath(UpdateFolder);
	QDir(ExtractRoot).removeRecursively();
	QDir().mkpath(ExtractRoot);

	QProcess ExtractProcess;
	const QString EscapedZipPath = QDir::toNativeSeparators(ZipPath).replace("'", "''");
	const QString EscapedExtractRoot = QDir::toNativeSeparators(ExtractRoot).replace("'", "''");
	QStringList ExtractArgs;
	ExtractArgs
		<< "-NoProfile"
		<< "-ExecutionPolicy" << "Bypass"
		<< "-Command" << QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
			.arg(EscapedZipPath, EscapedExtractRoot);
	ExtractProcess.start("powershell.exe", ExtractArgs);
	if (!ExtractProcess.waitForStarted(10000) || !ExtractProcess.waitForFinished(-1) || ExtractProcess.exitCode() != 0)
	{
		const QString PsError = QString::fromLocal8Bit(ExtractProcess.readAllStandardError());
		QMessageBox::warning(this, "Launcher Update",
			QString("Unable to prepare downloaded update files.\n\n%1").arg(PsError.trimmed().isEmpty() ? "Expand-Archive failed." : PsError.trimmed()));
		return false;
	}

	const QString RuntimeDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
	QString InstalledUpdaterPath = QDir::cleanPath(QString("%1/LauncherUpdater.exe").arg(RuntimeDir));
	if (!QFileInfo::exists(InstalledUpdaterPath))
		InstalledUpdaterPath = QDir::cleanPath(QString("%1/modtools/ModLauncherCustomRuntime/LauncherUpdater.exe").arg(mGamePath));

	if (!QFileInfo::exists(InstalledUpdaterPath))
	{
		QMessageBox::warning(this, "Launcher Update",
			QString("LauncherUpdater.exe was not found.\nExpected at:\n%1").arg(QDir::toNativeSeparators(InstalledUpdaterPath)));
		return false;
	}

	const QString RunnerPath = QDir::cleanPath(QString("%1/LauncherUpdater_runner_%2_%3.exe")
		.arg(QDir::tempPath())
		.arg(QCoreApplication::applicationPid())
		.arg(QDateTime::currentDateTimeUtc().toString("yyyyMMddhhmmsszzz")));
	QFile::remove(RunnerPath);
	if (!QFile::copy(InstalledUpdaterPath, RunnerPath))
	{
		QMessageBox::warning(this, "Launcher Update",
			QString("Unable to prepare updater runner:\n%1").arg(QDir::toNativeSeparators(RunnerPath)));
		return false;
	}

	const QString RestartPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
	const QStringList UpdaterArgs = QStringList()
		<< "--pid" << QString::number(QCoreApplication::applicationPid())
		<< "--install-dir" << QDir::toNativeSeparators(mGamePath)
		<< "--update-dir" << QDir::toNativeSeparators(UpdateFolder)
		<< "--restart" << RestartPath;

	if (!QProcess::startDetached(QDir::toNativeSeparators(RunnerPath), UpdaterArgs, QDir::toNativeSeparators(UpdateFolder)))
	{
		QMessageBox::warning(this, "Launcher Update", "Could not launch LauncherUpdater.exe.");
		return false;
	}

	return true;
}

void mlMainWindow::SetFooterUpdateState(const QString& Message, const QString& DownloadUrl, const QString& VersionLabel, const QString& ReleasePageUrl, bool UpdateAvailable)
{
	mAvailableUpdateUrl = DownloadUrl.trimmed();
	mAvailableUpdateVersion = VersionLabel.trimmed();
	mAvailableReleasePageUrl = ReleasePageUrl.trimmed();

	if (mFooterUpdateStatusLabel)
	{
		const QString FooterStateText = Message.trimmed().isEmpty() ? "Up to date" : Message.trimmed();
		mFooterUpdateStatusLabel->setText(QString("v%1 - %2").arg(kLauncherVersion, FooterStateText));
		mFooterUpdateStatusLabel->setStyleSheet(UpdateAvailable ? "color: #63d471; padding: 4px 8px; font-weight: 700;" : QString());
	}

	if (mFooterDownloadButton)
	{
		const bool CanDownload = !mAvailableUpdateVersion.isEmpty() && (!mAvailableUpdateUrl.isEmpty() || !mAvailableReleasePageUrl.isEmpty());
		const bool ShowDownloadButton = UpdateAvailable && CanDownload;
		mFooterDownloadButton->setVisible(ShowDownloadButton);
		if (CanDownload)
			mFooterDownloadButton->setText("Download");
	}

	if (mFooterRefreshButton)
	{
		const bool CanDownload = !mAvailableUpdateVersion.isEmpty() && (!mAvailableUpdateUrl.isEmpty() || !mAvailableReleasePageUrl.isEmpty());
		mFooterRefreshButton->setVisible(!(UpdateAvailable && CanDownload));
	}
}

void mlMainWindow::RecordUserActivity()
{
	if (!mStatsElapsedTimer.isValid())
		return;

	mLastActivityMs = mStatsElapsedTimer.elapsed();
}

void mlMainWindow::UpdateStatsTimers()
{
	if (!mStatsElapsedTimer.isValid())
		return;

	const qint64 NowMs = mStatsElapsedTimer.elapsed();
	if (NowMs <= mLastStatsTickMs)
		return;

	const qint64 DeltaSeconds = (NowMs - mLastStatsTickMs) / 1000;
	if (DeltaSeconds <= 0)
		return;

	AddStatSeconds("Stats/TotalOpenSeconds", DeltaSeconds);
	const qint64 IdleThresholdMs = 60000;
	const bool IsIdle = (NowMs - mLastActivityMs) >= IdleThresholdMs;
	AddStatSeconds(IsIdle ? "Stats/TotalIdleSeconds" : "Stats/TotalActiveSeconds", DeltaSeconds);
	mLastStatsTickMs += DeltaSeconds * 1000;
}

void mlMainWindow::FlushCurrentItemTime()
{
	if (!mStatsElapsedTimer.isValid() || mCurrentStatsItemKey.isEmpty() || mCurrentItemStartedMs <= 0)
		return;

	const qint64 DurationSeconds = qMax<qint64>(0, (mStatsElapsedTimer.elapsed() - mCurrentItemStartedMs) / 1000);
	if (DurationSeconds > 0)
		AddStatSeconds(QString("Stats/Items/%1/LauncherSeconds").arg(mCurrentStatsItemKey), DurationSeconds);

	mCurrentItemStartedMs = mStatsElapsedTimer.elapsed();
}

void mlMainWindow::SetCurrentStatsItem(QTreeWidgetItem* Item)
{
	if (!mStatsElapsedTimer.isValid())
		return;

	UpdateStatsTimers();
	FlushCurrentItemTime();
	mCurrentStatsItemKey.clear();
	if (Item && Item->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
		mCurrentStatsItemKey = NormalizeStatsItemKey(GetItemFavoriteKey(Item));
	mCurrentItemStartedMs = mStatsElapsedTimer.elapsed();
}

QString mlMainWindow::NormalizeStatsItemKey(const QString& FavoriteKey) const
{
	const QString Key = FavoriteKey.trimmed().toLower();
	if (Key.startsWith("mod:"))
	{
		const int SlashIndex = Key.indexOf('/');
		if (SlashIndex > 0)
			return Key.left(SlashIndex);
	}

	return Key;
}

void mlMainWindow::AddStatSeconds(const QString& Key, qint64 Seconds)
{
	if (Seconds <= 0)
		return;

	QSettings Settings;
	Settings.setValue(Key, Settings.value(Key, 0).toLongLong() + Seconds);
}

void mlMainWindow::IncrementStat(const QString& Key, qint64 Amount)
{
	if (Amount == 0)
		return;

	QSettings Settings;
	Settings.setValue(Key, Settings.value(Key, 0).toLongLong() + Amount);
}

qint64 mlMainWindow::StatValue(const QString& Key) const
{
	QSettings Settings;
	return Settings.value(Key, 0).toLongLong();
}

QString mlMainWindow::FormatDuration(qint64 Seconds) const
{
	Seconds = qMax<qint64>(0, Seconds);
	const qint64 Hours = Seconds / 3600;
	const qint64 Minutes = (Seconds % 3600) / 60;
	const qint64 RemainingSeconds = Seconds % 60;
	if (Hours > 0)
		return QString("%1h %2m %3s").arg(Hours).arg(Minutes).arg(RemainingSeconds);
	if (Minutes > 0)
		return QString("%1m %2s").arg(Minutes).arg(RemainingSeconds);
	return QString("%1s").arg(RemainingSeconds);
}

QString mlMainWindow::DisplayNameForStatsKey(const QString& FavoriteKey) const
{
	const QString DisplayName = DisplayNameForEntry(FavoriteKey);
	if (!DisplayName.isEmpty())
		return DisplayName;

	if (FavoriteKey.startsWith("map:"))
		return FavoriteKey.mid(4);
	if (FavoriteKey.startsWith("mod:"))
		return FavoriteKey.mid(4);
	return FavoriteKey;
}

void mlMainWindow::TrackProcessLifetime(QProcess* Process, const QString& CountKey, const QString& SecondsKey, const QString& PerItemSecondsPrefix, const QString& ItemKey)
{
	if (!Process)
		return;

	IncrementStat(CountKey);
	QElapsedTimer* Timer = new QElapsedTimer();
	Timer->start();
	connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
	connect(Process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int, QProcess::ExitStatus)
	{
		const qint64 DurationSeconds = qMax<qint64>(0, Timer->elapsed() / 1000);
		delete Timer;
		AddStatSeconds(SecondsKey, DurationSeconds);
		if (!PerItemSecondsPrefix.isEmpty() && !ItemKey.isEmpty())
			AddStatSeconds(QString("%1/%2").arg(PerItemSecondsPrefix, ItemKey.toLower()), DurationSeconds);
	});
}

void mlMainWindow::OnBuildCommandFinished(const QString& Program, const QStringList& Arguments, qint64 DurationMs, int ExitCode)
{
	if (ExitCode != 0)
		return;

	const QString ProgramLower = QFileInfo(Program).fileName().toLower();
	const qint64 DurationSeconds = qMax<qint64>(0, DurationMs / 1000);
	auto ArgumentValue = [&](const QString& SwitchName) -> QString
	{
		const int Index = Arguments.indexOf(SwitchName);
		if (Index >= 0 && Index + 1 < Arguments.count())
			return Arguments[Index + 1];
		return QString();
	};
	auto MapKeyFromArguments = [&]() -> QString
	{
		QString MapPath = ArgumentValue("-loadFrom");
		if (MapPath.isEmpty())
		{
			for (const QString& Argument : Arguments)
			{
				if (Argument.endsWith(".map", Qt::CaseInsensitive))
				{
					MapPath = Argument;
					break;
				}
			}
		}
		if (MapPath.isEmpty())
			return QString();
		return NormalizeStatsItemKey(QString("map:%1").arg(QFileInfo(MapPath).completeBaseName().toLower()));
	};
	auto LinkKeyFromArguments = [&]() -> QString
	{
		const QString ModSource = ArgumentValue("-modsource").trimmed().toLower();
		const QString FsGame = ArgumentValue("-fs_game").trimmed().toLower();
		if (!FsGame.isEmpty())
			return NormalizeStatsItemKey(QString("mod:%1/%2").arg(FsGame, ModSource));
		if (!ModSource.isEmpty())
			return NormalizeStatsItemKey(QString("map:%1").arg(ModSource));
		return QString();
	};
	auto StorePerItemBuildStat = [&](const QString& ItemKey, const QString& CountPrefix, const QString& SecondsPrefix, const QString& LastUtcPrefix)
	{
		if (ItemKey.isEmpty())
			return;
		IncrementStat(QString("%1/%2").arg(CountPrefix, ItemKey));
		AddStatSeconds(QString("%1/%2").arg(SecondsPrefix, ItemKey), DurationSeconds);
		QSettings().setValue(QString("%1/%2").arg(LastUtcPrefix, ItemKey), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	};
	if (ProgramLower == "compiler_modtools.exe" || ProgramLower == "cod2map64.exe")
	{
		IncrementStat("Stats/TotalCompiles");
		AddStatSeconds("Stats/TotalCompileSeconds", DurationSeconds);
		StorePerItemBuildStat(MapKeyFromArguments(), "Stats/ItemsCompileCount", "Stats/ItemsCompileSeconds", "Stats/ItemsLastCompileUtc");
	}
	else if (ProgramLower == "linker_modtools.exe")
	{
		IncrementStat("Stats/TotalLinks");
		AddStatSeconds("Stats/TotalLinkSeconds", DurationSeconds);
		StorePerItemBuildStat(LinkKeyFromArguments(), "Stats/ItemsLinkCount", "Stats/ItemsLinkSeconds", "Stats/ItemsLastLinkUtc");
	}
	else if (ProgramLower == "light.exe" || ProgramLower == "radiant_modtools.exe")
	{
		IncrementStat("Stats/TotalLights");
		AddStatSeconds("Stats/TotalLightSeconds", DurationSeconds);
	}
	else if (IsGameLaunchCommand(Program, Arguments))
		IncrementStat("Stats/TotalLauncherRuns");
}

void mlMainWindow::StatsTick()
{
	UpdateStatsTimers();
}

QString mlMainWindow::ResolveSteamExecutablePath() const
{
	QSettings SteamUser(QString("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QSettings::NativeFormat);
	const QString SteamExe = QDir::fromNativeSeparators(SteamUser.value("SteamExe").toString().trimmed());
	if (!SteamExe.isEmpty() && QFileInfo(SteamExe).exists())
		return SteamExe;

	QSettings SteamMachine(QString("HKEY_LOCAL_MACHINE\\Software\\WOW6432Node\\Valve\\Steam"), QSettings::NativeFormat);
	const QString InstallPath = QDir::fromNativeSeparators(SteamMachine.value("InstallPath").toString().trimmed());
	if (!InstallPath.isEmpty())
	{
		const QString Candidate = QDir::cleanPath(InstallPath + "/Steam.exe");
		if (QFileInfo(Candidate).exists())
			return Candidate;
	}

	const QString ProgramFilesX86 = QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("ProgramFiles(x86)")));
	if (!ProgramFilesX86.isEmpty())
	{
		const QString Candidate = QDir::cleanPath(ProgramFilesX86 + "/Steam/Steam.exe");
		if (QFileInfo(Candidate).exists())
			return Candidate;
	}

	const QString ProgramFiles = QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("ProgramFiles")));
	if (!ProgramFiles.isEmpty())
	{
		const QString Candidate = QDir::cleanPath(ProgramFiles + "/Steam/Steam.exe");
		if (QFileInfo(Candidate).exists())
			return Candidate;
	}

	return QString("Steam.exe");
}

QPair<QString, QStringList> mlMainWindow::CreateGameLaunchCommand(const QString& FsGame, const QString& MapName) const
{
	QStringList Args;
	if (!mRunDvars.isEmpty())
		Args << mRunDvars;
	if (!FsGame.isEmpty())
		Args << "+set" << "fs_game" << FsGame;
	if (!MapName.isEmpty())
		Args << "+devmap" << MapName;
	if (mQuickLaunchWidget && mQuickLaunchWidget->IsWorkshopSelection())
	{
		const QString WorkshopId = mQuickLaunchWidget->currentWorkshopId().trimmed();
		if (Args.contains("+set") && Args.contains("fs_game"))
		{
			const int FsGameIdx = Args.indexOf("fs_game");
			if (FsGameIdx >= 1)
			{
				Args.removeAt(FsGameIdx - 1);
				Args.removeAt(FsGameIdx - 1);
			}
		}
		Args << "+set" << "fs_game" << "mod";
		if (!WorkshopId.isEmpty())
			Args << "+set" << "workshopid" << WorkshopId;
	}

	const QString ExtraOptions = mRunOptionsWidget ? mRunOptionsWidget->text() : QString();
	if (!ExtraOptions.isEmpty())
		Args << ExtraOptions.split(' ', Qt::SkipEmptyParts);

	if (ShouldUseSteamOnlineLaunch())
		return qMakePair(ResolveSteamExecutablePath(), QStringList() << "-applaunch" << QString::number(AppId) << Args);

	return qMakePair(QString("%1/BlackOps3.exe").arg(mGamePath), Args);
}

void mlMainWindow::SetTreeItemChecked(QTreeWidgetItem* Item, Qt::CheckState State, bool PropagateChildren)
{
	if (!Item)
		return;

	const QString FavoriteKey = GetItemFavoriteKey(Item);
	if (!FavoriteKey.isEmpty())
		ApplyCheckStateForKey(FavoriteKey, State, Item);

	Item->setData(0, ML_ITEM_CHECKSTATE_ROLE, State);
	SyncItemCheckWidget(Item);

	if (PropagateChildren)
	{
		for (int ChildIdx = 0; ChildIdx < Item->childCount(); ChildIdx++)
			SetTreeItemChecked(Item->child(ChildIdx), State, true);
	}

	UpdateParentCheckState(Item);
	UpdateBuildActionButtons();
}

bool mlMainWindow::SupportsDisplayName(int ItemType) const
{
	return ItemType == ML_ITEM_MAP || ItemType == ML_ITEM_MOD_GROUP;
}

QString mlMainWindow::DisplayNameForEntry(const QString& FavoriteKey) const
{
	if (FavoriteKey.isEmpty())
		return QString();

	QSettings Settings;
	return Settings.value(QString("DisplayNames/%1").arg(FavoriteKey.toLower())).toString().trimmed();
}

void mlMainWindow::SetDisplayNameForEntry(const QString& FavoriteKey, const QString& DisplayName)
{
	if (FavoriteKey.isEmpty())
		return;

	QSettings Settings;
	const QString SettingKey = QString("DisplayNames/%1").arg(FavoriteKey.toLower());
	const QString TrimmedName = DisplayName.trimmed();
	if (TrimmedName.isEmpty())
		Settings.remove(SettingKey);
	else
		Settings.setValue(SettingKey, TrimmedName);
}

QString mlMainWindow::DisplayColorForEntry(const QString& FavoriteKey) const
{
	if (FavoriteKey.isEmpty())
		return QString();

	QSettings Settings;
	return NormalizedStoredColor(Settings.value(QString("DisplayColors/%1").arg(FavoriteKey.toLower())).toString());
}

void mlMainWindow::SetDisplayColorForEntry(const QString& FavoriteKey, const QString& ColorValue)
{
	if (FavoriteKey.isEmpty())
		return;

	QSettings Settings;
	const QString SettingKey = QString("DisplayColors/%1").arg(FavoriteKey.toLower());
	const QString NormalizedColor = NormalizedStoredColor(ColorValue);
	if (NormalizedColor.isEmpty())
		Settings.remove(SettingKey);
	else
		Settings.setValue(SettingKey, NormalizedColor);
}

bool mlMainWindow::PromptForDisplayName(int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey)
{
	if (!SupportsDisplayName(ItemType) || FavoriteKey.isEmpty())
		return false;

	const QString InternalName = DisplayTextForItem(ItemType, ContainerName, EntryName);
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Display Options");
	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QLabel* Intro = new QLabel(QString("Customize how '%1' appears in the launcher list.").arg(InternalName), &Dialog);
	Intro->setWordWrap(true);
	Layout->addWidget(Intro);

	QFormLayout* FormLayout = new QFormLayout();
	Layout->addLayout(FormLayout);

	QLineEdit* DisplayNameEdit = new QLineEdit(DisplayNameForEntry(FavoriteKey), &Dialog);
	FormLayout->addRow("Display name:", DisplayNameEdit);

	QWidget* ColorWidget = new QWidget(&Dialog);
	QHBoxLayout* ColorLayout = new QHBoxLayout(ColorWidget);
	ColorLayout->setContentsMargins(0, 0, 0, 0);
	ColorLayout->setSpacing(6);
	QLineEdit* ColorEdit = new QLineEdit(DisplayColorForEntry(FavoriteKey), ColorWidget);
	ColorEdit->setPlaceholderText("#ff8a2a");
	QPushButton* ColorBrowseButton = new QPushButton(ColorWidget);
	ColorBrowseButton->setFixedSize(26, 26);
	ColorBrowseButton->setToolTip("Pick selection color");
	QToolButton* ColorHelpButton = new QToolButton(ColorWidget);
	ColorHelpButton->setText("?");
	ColorHelpButton->setToolTip("Colors only change how this map or mod appears inside the launcher list.");
	QPushButton* ClearColorButton = new QPushButton("Clear", ColorWidget);
	ColorLayout->addWidget(ColorEdit, 1);
	ColorLayout->addWidget(ColorBrowseButton);
	ColorLayout->addWidget(ColorHelpButton);
	ColorLayout->addWidget(ClearColorButton);
	FormLayout->addRow("Selection color:", ColorWidget);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	Layout->addWidget(ButtonBox);

	auto RefreshColorButton = [=]()
	{
		const QString NormalizedColor = NormalizedStoredColor(ColorEdit->text());
		const QString SafeColor = NormalizedColor.isEmpty() ? QString("#444444") : NormalizedColor;
		ColorBrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(SafeColor));
	};

	connect(ColorBrowseButton, &QPushButton::clicked, &Dialog, [&, ColorEdit]()
	{
		QColor Color = QColorDialog::getColor(QColor(ColorEdit->text()), &Dialog, "Select Selection Color");
		if (Color.isValid())
			ColorEdit->setText(Color.name(QColor::HexRgb));
	});
	connect(ColorEdit, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshColorButton(); });
	connect(ClearColorButton, &QPushButton::clicked, &Dialog, [=]() { ColorEdit->clear(); });
	connect(ColorHelpButton, &QToolButton::clicked, &Dialog, [&, this]()
	{
		QMessageBox::information(&Dialog, "Selection Color", "Use a custom color to make a favorite map or mod stand out in the launcher list. This only changes the launcher UI, not the in-game name.");
	});
	RefreshColorButton();

	if (Dialog.exec() != QDialog::Accepted)
		return false;

	SetDisplayNameForEntry(FavoriteKey, DisplayNameEdit->text().trimmed());
	SetDisplayColorForEntry(FavoriteKey, ColorEdit->text());
	return true;
}

QList<QTreeWidgetItem*> mlMainWindow::CollectTargetItems(bool* HasMapSelection) const
{
	QList<QTreeWidgetItem*> TargetItems;
	QSet<QString> TargetKeys;
	bool FoundMapSelection = false;

	std::function<void(QTreeWidgetItem*)> CollectChecked = [&](QTreeWidgetItem* Parent)
	{
		for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = Parent->child(ChildIdx);
			if (ItemCheckState(Child) == Qt::Checked && Child->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
			{
				const QString FavoriteKey = GetItemFavoriteKey(Child);
				if (!TargetKeys.contains(FavoriteKey))
				{
					TargetItems.append(Child);
					TargetKeys.insert(FavoriteKey);
					FoundMapSelection |= (Child->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP);
				}
			}
			else
				CollectChecked(Child);
		}
	};
	CollectChecked(mFileListWidget->invisibleRootItem());

	if (TargetItems.isEmpty())
	{
		QTreeWidgetItem* CurrentItem = mFileListWidget->currentItem();
		if (!CurrentItem)
		{
			QList<QTreeWidgetItem*> SelectedItems = mFileListWidget->selectedItems();
			if (!SelectedItems.isEmpty())
				CurrentItem = SelectedItems[0];
		}

		if (CurrentItem && CurrentItem->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
		{
			TargetItems.append(CurrentItem);
			FoundMapSelection = (CurrentItem->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP);
		}
	}

	if (HasMapSelection)
		*HasMapSelection = FoundMapSelection;

	return TargetItems;
}

bool mlMainWindow::MapMatchesCurrentTab(const QString& MapName) const
{
	if (!mCategoryTabs)
		return true;
	const QString ActiveTabKey = CurrentCategoryTabKey(mCategoryTabs);
	if (ActiveTabKey == "zm-maps")
		return MapName.startsWith("zm_", Qt::CaseInsensitive);
	if (ActiveTabKey == "mp-maps")
		return MapName.startsWith("mp_", Qt::CaseInsensitive);
	return true;
}

QString mlMainWindow::DisplayTextForItem(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	if (ItemType == ML_ITEM_MOD)
	{
		QString ZoneDescription;
		if (EntryName.compare("core_mod", Qt::CaseInsensitive) == 0)
			ZoneDescription = "Lobby .zone file";
		else if (EntryName.compare("zm_mod", Qt::CaseInsensitive) == 0)
			ZoneDescription = "Zombie .zone file";
		else if (EntryName.compare("mp_mod", Qt::CaseInsensitive) == 0)
			ZoneDescription = "Multiplayer .zone file";
		else if (EntryName.compare("cp_mod", Qt::CaseInsensitive) == 0)
			ZoneDescription = "Campaign .zone file";

		return ZoneDescription.isEmpty() ? EntryName : QString("%1  (%2)").arg(EntryName, ZoneDescription);
	}
	return EntryName;
}

QWidget* mlMainWindow::CreateItemTitleWidget(QTreeWidgetItem* Item, int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey, const QString& BaseText)
{
	const bool IsChildRow = Item && Item->parent() && Item->parent()->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN;
	const bool UseClassicChrome = ThemeUsesClassicChrome(mThemeMode);
	const bool UseUpdatedChrome = ThemeUsesUpdatedChrome(mThemeMode);
	const bool UseDarkModernChrome = ThemeUsesDarkModernChrome(mThemeMode);
	const int RowHeight = UseClassicChrome ? 22 : (UseUpdatedChrome ? 42 : 44);
	const int PanelHeight = UseClassicChrome ? 14 : (UseUpdatedChrome ? 32 : 34);
	const int VerticalMargin = UseClassicChrome ? 1 : (UseUpdatedChrome ? 3 : 4);
	const int ContentSpacing = UseClassicChrome ? 1 : 6;
	HoverRevealWidget* RowWidget = new HoverRevealWidget(mFileListWidget);
	RowWidget->setObjectName("ItemRowWidget");
	RowWidget->setAttribute(Qt::WA_StyledBackground, true);
	RowWidget->setFixedHeight(RowHeight);
	QHBoxLayout* RowLayout = new QHBoxLayout(RowWidget);
	RowLayout->setContentsMargins(0, VerticalMargin, 0, VerticalMargin);
	RowLayout->setSpacing(UseDarkModernChrome ? 4 : 0);

	HoverRevealWidget* Widget = new HoverRevealWidget(RowWidget);
	Widget->setObjectName("ItemTitleWidget");
	Widget->setAttribute(Qt::WA_StyledBackground, true);
	Widget->setProperty("childRow", IsChildRow);
	Widget->setProperty("selected", mFileListWidget && mFileListWidget->currentItem() == Item);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	Widget->setFixedHeight(PanelHeight);
	QHBoxLayout* Layout = new QHBoxLayout(Widget);
	Layout->setContentsMargins(UseClassicChrome ? 1 : 9, UseClassicChrome ? 0 : 2, UseClassicChrome ? 1 : 7, UseClassicChrome ? 0 : 2);
	Layout->setSpacing(ContentSpacing);

	QCheckBox* CheckBox = new QCheckBox(Widget);
	CheckBox->setObjectName("ItemSelectCheckBox");
	CheckBox->setChecked(ItemCheckState(Item) == Qt::Checked);
	connect(CheckBox, &QCheckBox::toggled, this, [=](bool Checked)
	{
		SetTreeItemChecked(Item, Checked ? Qt::Checked : Qt::Unchecked, Item->childCount() > 0);
		mFileListWidget->setCurrentItem(Item);
	});
	Layout->addWidget(CheckBox, 0, Qt::AlignVCenter);
	if (UseClassicChrome)
		Layout->addSpacing(3);

	QPushButton* NameButton = new QPushButton(Widget);
	NameButton->setObjectName("ItemNameButton");
	NameButton->setFlat(true);
	NameButton->setCursor(Qt::PointingHandCursor);
	NameButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
	const QString CustomDisplayName = SupportsDisplayName(ItemType) ? DisplayNameForEntry(FavoriteKey) : QString();
	const bool HasCustomDisplayName = !CustomDisplayName.isEmpty();
	NameButton->setText(CustomDisplayName);
	if (NameButton->text().isEmpty())
		NameButton->setText(BaseText);
	const QString DisplayColor = SupportsDisplayName(ItemType) ? DisplayColorForEntry(FavoriteKey) : QString();
	QString NameButtonStyle;
	if (!DisplayColor.isEmpty())
		NameButtonStyle = QString("color:%1;").arg(DisplayColor);
	if (HasCustomDisplayName)
		NameButtonStyle += "font-weight:700;";
	if (!NameButtonStyle.isEmpty())
		NameButton->setStyleSheet(NameButtonStyle);
	connect(NameButton, &QPushButton::clicked, this, [=]()
	{
		mFileListWidget->setCurrentItem(Item);
		if (PromptForDisplayName(ItemType, ContainerName, EntryName, FavoriteKey))
			PopulateFileList();
	});
	Layout->addWidget(NameButton, 0, Qt::AlignVCenter);

	if (SupportsDisplayName(ItemType) && !FavoriteKey.isEmpty() && DisplayNameForEntry(FavoriteKey).isEmpty())
	{
		QToolButton* DisplayNameButton = new QToolButton(Widget);
		DisplayNameButton->setObjectName("DisplayNameAddButton");
			DisplayNameButton->setText("+");
			DisplayNameButton->setToolTip("Add display name");
			DisplayNameButton->setCursor(Qt::PointingHandCursor);
			DisplayNameButton->setAutoRaise(false);
				DisplayNameButton->setFixedSize(UseClassicChrome ? 10 : 20, UseClassicChrome ? 10 : 20);
		connect(DisplayNameButton, &QToolButton::clicked, this, [=]()
		{
			mFileListWidget->setCurrentItem(Item);
			if (PromptForDisplayName(ItemType, ContainerName, EntryName, FavoriteKey))
				PopulateFileList();
		});
		Layout->addWidget(DisplayNameButton, 0, Qt::AlignVCenter);
		Widget->SetRevealWidget(DisplayNameButton);
	}

	if (SupportsDisplayName(ItemType))
	{
		QLabel* InternalNameLabel = new QLabel(BaseText, Widget);
		InternalNameLabel->setObjectName("ItemInternalNameLabel");
		InternalNameLabel->setVisible(!DisplayNameForEntry(FavoriteKey).isEmpty());
		Layout->addWidget(InternalNameLabel, 0, Qt::AlignVCenter);
	}

	const bool ShowItemTypeTags = QSettings().value("ShowItemTypeTags", true).toBool();
	if (ShowItemTypeTags && (ItemType == ML_ITEM_MAP || ItemType == ML_ITEM_MOD_GROUP))
	{
		QLabel* TypeTag = new QLabel(ItemType == ML_ITEM_MAP ? "Map" : "Mod", Widget);
		TypeTag->setObjectName("ItemTypeTag");
		TypeTag->setProperty("itemType", ItemType == ML_ITEM_MAP ? "map" : "mod");
		Layout->addWidget(TypeTag, 0, Qt::AlignVCenter);
	}
	Layout->addStretch(1);
	if (ItemType == ML_ITEM_MAP || ItemType == ML_ITEM_MOD_GROUP)
	{
		QLabel* SizeLabel = new QLabel(FormatDataSizeBytes(ItemDiskSizeBytes(ItemType, ContainerName, EntryName)), Widget);
		SizeLabel->setObjectName("ItemSizeLabel");
		SizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		SizeLabel->setToolTip(QString("Total size on disk for %1").arg(QDir::toNativeSeparators(ItemContentRoot(ItemType, ContainerName, EntryName))));
		Layout->addWidget(SizeLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
	}

	RowLayout->addWidget(Widget, 1);
	if (QWidget* ActionsWidget = CreateQuickActionsWidget(Item, ItemType, ContainerName, EntryName, FavoriteKey, IsChildRow))
		RowLayout->addWidget(ActionsWidget, 0);
	return RowWidget;
}

QWidget* mlMainWindow::CreateQuickActionsWidget(QTreeWidgetItem* Item, int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey, bool IsChildRow)
{
	Q_UNUSED(Item);
	Q_UNUSED(ItemType);
	Q_UNUSED(ContainerName);
	Q_UNUSED(EntryName);
	Q_UNUSED(FavoriteKey);
	Q_UNUSED(IsChildRow);
	return NULL;
}

void mlMainWindow::SyncItemSelectionWidget(QTreeWidgetItem* Item) const
{
	if (!mFileListWidget || !Item)
		return;

	QWidget* RowWidget = mFileListWidget->itemWidget(Item, 0);
	if (!RowWidget)
		return;

	const bool Selected = (mFileListWidget->currentItem() == Item);
	auto ApplySelectionState = [Selected](QWidget* Widget)
	{
		if (!Widget)
			return;
		Widget->setProperty("selected", Selected);
		Widget->style()->unpolish(Widget);
		Widget->style()->polish(Widget);
		Widget->update();
	};

	QWidget* TitleWidget = RowWidget->findChild<QWidget*>("ItemTitleWidget");
	ApplySelectionState(TitleWidget);
	ApplySelectionState(RowWidget->findChild<QWidget*>("QuickActionStrip"));
}

void mlMainWindow::PopulatePinnedRoot(QTreeWidgetItem* RootItem, const QStringList& Keys, const QHash<QString, QVariantMap>& Lookup)
{
	QSet<QString> AddedEntries;
	const int PinnedRowHeight = TreeRowHeightForTheme(mThemeMode, true);
	auto AddPinnedItem = [&](QTreeWidgetItem* Parent, const QVariantMap& ItemData, const QString& Key, int Height) -> QTreeWidgetItem*
	{
		QTreeWidgetItem* Item = new QTreeWidgetItem(Parent, QStringList() << ItemData.value("display").toString());
		const int ItemType = ItemData.value("type").toInt();
		const QString ContainerName = ItemData.value("container").toString();
		const QString EntryName = ItemData.value("entry").toString();
		Item->setData(0, ML_ITEM_CHECKSTATE_ROLE, CheckStateForKey(Key));
		Item->setData(0, Qt::UserRole, ItemType);
		Item->setData(0, ML_ITEM_CONTAINER_ROLE, ContainerName);
		Item->setData(0, ML_ITEM_NAME_ROLE, EntryName);
		Item->setData(0, ML_ITEM_FAVORITE_ROLE, Key);
		Item->setFlags(Item->flags() & ~Qt::ItemIsUserCheckable);
		Item->setForeground(0, QBrush(Qt::transparent));
		Item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
		Item->setSizeHint(0, QSize(0, Height));
		mFileListWidget->setItemWidget(Item, 0, CreateItemTitleWidget(Item, ItemType, ContainerName, EntryName, Key, ItemData.value("display").toString()));
		return Item;
	};

	for (const QString& Key : Keys)
	{
		const QString NormalizedKey = Key.toLower();
		if (AddedEntries.contains(NormalizedKey) || !Lookup.contains(Key))
			continue;

		const QVariantMap ItemData = Lookup.value(Key);
		if (ItemData.value("type").toInt() == ML_ITEM_MOD_GROUP)
		{
			QTreeWidgetItem* GroupItem = AddPinnedItem(RootItem, ItemData, Key, PinnedRowHeight);
			GroupItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
			AddedEntries.insert(NormalizedKey);

			for (const QString& ZoneName : ModZoneNames(ItemData.value("container").toString()))
			{
				const QString ChildKey = QString("mod:%1/%2").arg(ItemData.value("container").toString().toLower(), ZoneName.toLower());
				if (!Lookup.contains(ChildKey))
					continue;

				AddPinnedItem(GroupItem, Lookup.value(ChildKey), ChildKey, PinnedRowHeight);
				AddedEntries.insert(ChildKey.toLower());
			}

			GroupItem->setExpanded(true);
			continue;
		}

		AddPinnedItem(RootItem, ItemData, Key, PinnedRowHeight);
		AddedEntries.insert(NormalizedKey);
	}
}

void mlMainWindow::RunQuickAction(int ItemType, const QString& ContainerName, const QString& EntryName, bool LinkAction, bool RunAction)
{
	if (mBuildThread)
		return;

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));

	QStringList LanguageArgs;
	if (mBuildLanguage == "All")
	{
		for (const QString& Language : gLanguages)
			LanguageArgs << "-language" << Language;
	}
	else
		LanguageArgs << "-language" << mBuildLanguage;

	if (LinkAction)
	{
		if (ItemType == ML_ITEM_MAP)
		{
			Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-modsource" << EntryName));
		}
		else if (ItemType == ML_ITEM_MOD)
		{
			Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ContainerName << "-modsource" << EntryName));
		}
		else if (ItemType == ML_ITEM_MOD_GROUP)
		{
			for (const QString& ZoneName : ModZoneNames(ContainerName))
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ContainerName << "-modsource" << ZoneName));
		}
	}

	if (RunAction)
		QueueGameStatsForItem(ItemType == ML_ITEM_MAP ? QString("map:%1").arg(EntryName.toLower()) : QString("mod:%1").arg(ContainerName.toLower()));

	if (RunAction)
	{
		Commands.append(CreateGameLaunchCommand(ItemType == ML_ITEM_MAP ? EntryName : ContainerName, ItemType == ML_ITEM_MAP ? EntryName : QString()));
	}

	if (Commands.isEmpty())
		return;

	TouchRecentEntry(RecentEntryForItem(ItemType, ContainerName, EntryName));
	if (LinkAction)
		SetActiveBuildButton(BuildAllLanguages);
	StartBuildThread(Commands);
}

QStringList mlMainWindow::DetectBuiltLanguages(const QString& ContentRoot) const
{
	QStringList BuiltLanguages;
	for (int LanguageIdx = 0; LanguageIdx < ARRAYSIZE(gLanguages); LanguageIdx++)
	{
		const QString Language = gLanguages[LanguageIdx];
		if (QDir(QString("%1/%2").arg(ContentRoot, Language)).exists())
			BuiltLanguages << Language;
	}
	return BuiltLanguages;
}

bool mlMainWindow::HasOnlyEnglishBuild(const QString& ContentRoot) const
{
	QStringList BuiltLanguages = DetectBuiltLanguages(ContentRoot);
	BuiltLanguages.removeDuplicates();
	return !BuiltLanguages.isEmpty() && BuiltLanguages.count() == 1 && BuiltLanguages.contains("english");
}

void mlMainWindow::UpdateBackgroundOverlays()
{
	auto ApplyOverlay = [](QLabel* Overlay, QWidget* Viewport, const QString& CachePath)
	{
		if (!Overlay || !Viewport)
			return;

		Overlay->setGeometry(Viewport->rect());
		if (CachePath.isEmpty() || !QFileInfo(CachePath).isFile())
		{
			Overlay->clear();
			Overlay->hide();
			return;
		}

		const QPixmap Pixmap(CachePath);
		if (Pixmap.isNull())
		{
			Overlay->clear();
			Overlay->hide();
			return;
		}

		Overlay->setPixmap(Pixmap.scaled(Viewport->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
		Overlay->show();
		Overlay->lower();
	};

	ApplyOverlay(mAssetTreeBackgroundOverlay, mFileListWidget ? mFileListWidget->viewport() : NULL, mAssetTreeBackgroundCachePath);
	ApplyOverlay(mOutputBackgroundOverlay, mOutputWidget && mOutputWidget->isVisible() ? mOutputWidget->viewport() : NULL, mLogBackgroundCachePath);
}

void mlMainWindow::PopulateFileList()
{
	AppendStartupTrace("PopulateFileList:start");
	mFileListWidget->clear();
	QSettings Settings;
	const QStringList Favorites = FavoriteEntries();
	const QStringList Recents = RecentEntries();
	QHash<QString, QVariantMap> Lookup;
	const QString ActiveTab = CurrentCategoryTabKey(mCategoryTabs);
	const int StandardRowHeight = TreeRowHeightForTheme(mThemeMode, false);
	QTreeWidgetItem* RecentsRootItem = NULL;
	QTreeWidgetItem* FavoritesRootItem = NULL;
	QTreeWidgetItem* MapsRootItem = NULL;
	QTreeWidgetItem* ModsRootItem = NULL;

	auto ConfigureItem = [&](QTreeWidgetItem* Item, int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey, const QString& DisplayText, int Height)
	{
		Item->setText(0, DisplayText);
		Item->setData(0, ML_ITEM_CHECKSTATE_ROLE, CheckStateForKey(FavoriteKey));
		Item->setData(0, Qt::UserRole, ItemType);
		Item->setData(0, ML_ITEM_CONTAINER_ROLE, ContainerName);
		Item->setData(0, ML_ITEM_NAME_ROLE, EntryName);
		Item->setData(0, ML_ITEM_FAVORITE_ROLE, FavoriteKey);
		Item->setFlags(Item->flags() & ~Qt::ItemIsUserCheckable);
		Item->setForeground(0, QBrush(Qt::transparent));
		Item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
		Item->setSizeHint(0, QSize(0, Height));
		if (ItemType != ML_ITEM_UNKNOWN)
			Lookup.insert(FavoriteKey, QVariantMap{ { "type", ItemType }, { "container", ContainerName }, { "entry", EntryName }, { "display", DisplayText } });
		mFileListWidget->setItemWidget(Item, 0, CreateItemTitleWidget(Item, ItemType, ContainerName, EntryName, FavoriteKey, DisplayText));
	};
	auto ConfigureSectionRoot = [&](QTreeWidgetItem* Item, const QString& Label)
	{
		if (!Item)
			return;

		Item->setText(0, Label);
		Item->setData(0, Qt::UserRole, ML_ITEM_UNKNOWN);
		Item->setFlags(Item->flags() & ~Qt::ItemIsUserCheckable);
		Item->setForeground(0, QBrush(Qt::transparent));
		Item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

		QWidget* HeaderWidget = new QWidget(mFileListWidget);
		HeaderWidget->setObjectName("ItemRowWidget");
		HeaderWidget->setAttribute(Qt::WA_StyledBackground, true);
		QHBoxLayout* HeaderLayout = new QHBoxLayout(HeaderWidget);
		HeaderLayout->setContentsMargins(12, 4, 8, 4);
		HeaderLayout->setSpacing(0);
		QLabel* HeaderLabel = new QLabel(Label, HeaderWidget);
		QFont HeaderFont = HeaderLabel->font();
		HeaderFont.setBold(true);
		HeaderLabel->setFont(HeaderFont);
		HeaderLabel->setStyleSheet("background: transparent;");
		HeaderLayout->addWidget(HeaderLabel, 0, Qt::AlignVCenter | Qt::AlignLeft);
		HeaderLayout->addStretch(1);
		Item->setSizeHint(0, QSize(0, StandardRowHeight));
		mFileListWidget->setItemWidget(Item, 0, HeaderWidget);
	};

	QString UserMapsFolder = QDir::cleanPath(QString("%1/usermaps/").arg(mGamePath));
	QStringList UserMaps = QDir(UserMapsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	if (ActiveTab == "all")
	{
		MapsRootItem = new QTreeWidgetItem(mFileListWidget, QStringList() << "Maps");
		ModsRootItem = new QTreeWidgetItem(mFileListWidget, QStringList() << "Mods");
		MapsRootItem->setFirstColumnSpanned(true);
		ModsRootItem->setFirstColumnSpanned(true);
		ConfigureSectionRoot(MapsRootItem, "Maps");
		ConfigureSectionRoot(ModsRootItem, "Mods");
	}

	for (QString MapName : UserMaps)
	{
		QString ZoneFileName = QString("%1/%2/zone_source/%3.zone").arg(UserMapsFolder, MapName, MapName);
		QString FavoriteKey = QString("map:%1").arg(MapName.toLower());

		if (QFileInfo(ZoneFileName).isFile())
		{
			Lookup.insert(FavoriteKey, QVariantMap{ { "type", ML_ITEM_MAP }, { "container", MapName }, { "entry", MapName }, { "display", MapName } });
			if (ActiveTab == "all")
			{
				QTreeWidgetItem* MapItem = new QTreeWidgetItem(MapsRootItem, QStringList() << MapName);
				ConfigureItem(MapItem, ML_ITEM_MAP, MapName, MapName, FavoriteKey, MapName, StandardRowHeight);
			}
			else if (ActiveTab == "zm-maps" || ActiveTab == "mp-maps")
			{
				if (!MapMatchesCurrentTab(MapName))
					continue;
				QTreeWidgetItem* MapItem = new QTreeWidgetItem(mFileListWidget, QStringList() << MapName);
				ConfigureItem(MapItem, ML_ITEM_MAP, MapName, MapName, FavoriteKey, MapName, StandardRowHeight);
			}
			else if (ActiveTab.startsWith("custom-"))
			{
				const QStringList TabItems = GetCustomTabItems(ActiveTab);
				if (!TabItems.contains(FavoriteKey, Qt::CaseInsensitive))
					continue;
				QTreeWidgetItem* MapItem = new QTreeWidgetItem(mFileListWidget, QStringList() << MapName);
				ConfigureItem(MapItem, ML_ITEM_MAP, MapName, MapName, FavoriteKey, MapName, StandardRowHeight);
			}
		}
	}

	QString ModsFolder = QDir::cleanPath(QString("%1/mods/").arg(mGamePath));
	QStringList Mods = QDir(ModsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	const char* Files[4] = { "core_mod", "mp_mod", "cp_mod", "zm_mod" };

	for (QString ModName : Mods)
	{
		QTreeWidgetItem* ParentItem = NULL;
		const QString GroupFavoriteKey = QString("mod:%1").arg(ModName.toLower());
		Lookup.insert(GroupFavoriteKey, QVariantMap{ { "type", ML_ITEM_MOD_GROUP }, { "container", ModName }, { "entry", ModName }, { "display", ModName } });

		for (int FileIdx = 0; FileIdx < 4; FileIdx++)
		{
			QString ZoneFileName = QString("%1/%2/zone_source/%3.zone").arg(ModsFolder, ModName, Files[FileIdx]);

			if (QFileInfo(ZoneFileName).isFile())
			{
				QString FavoriteKey = QString("mod:%1/%2").arg(ModName.toLower(), QString(Files[FileIdx]).toLower());
				Lookup.insert(FavoriteKey, QVariantMap{ { "type", ML_ITEM_MOD }, { "container", ModName }, { "entry", QString(Files[FileIdx]) }, { "display", QString(Files[FileIdx]) } });

				if (ActiveTab != "mods" && ActiveTab != "all" && !ActiveTab.startsWith("custom-"))
					continue;

				if (ActiveTab.startsWith("custom-"))
				{
					const QStringList TabItems = GetCustomTabItems(ActiveTab);
					if (!TabItems.contains(GroupFavoriteKey, Qt::CaseInsensitive))
						continue;
				}

				if (!ParentItem)
				{
					if (ActiveTab == "all")
						ParentItem = new QTreeWidgetItem(ModsRootItem, QStringList() << ModName);
					else
						ParentItem = new QTreeWidgetItem(mFileListWidget, QStringList() << ModName);
					ConfigureItem(ParentItem, ML_ITEM_MOD_GROUP, ModName, ModName, GroupFavoriteKey, ModName, StandardRowHeight);
					ParentItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
					ParentItem->setExpanded(true);
				}

				QTreeWidgetItem* ModItem = new QTreeWidgetItem(ParentItem, QStringList() << Files[FileIdx]);
				ConfigureItem(ModItem, ML_ITEM_MOD, ModName, Files[FileIdx], FavoriteKey, QString(Files[FileIdx]), StandardRowHeight);
			}
		}
	}

	if (ActiveTab == "recent")
		PopulatePinnedRoot(mFileListWidget->invisibleRootItem(), Recents, Lookup);
	else if (ActiveTab == "favorites")
		PopulatePinnedRoot(mFileListWidget->invisibleRootItem(), Favorites, Lookup);
	else if (ActiveTab == "all")
	{
		auto RemoveRootIfEmpty = [&](QTreeWidgetItem*& RootItem)
		{
			if (!RootItem || RootItem->childCount() > 0)
				return;

			const int RootIndex = mFileListWidget->indexOfTopLevelItem(RootItem);
			if (RootIndex >= 0)
				delete mFileListWidget->takeTopLevelItem(RootIndex);
				RootItem = NULL;
			};
		RemoveRootIfEmpty(MapsRootItem);
		RemoveRootIfEmpty(ModsRootItem);
		if (MapsRootItem)
			MapsRootItem->sortChildren(0, Qt::AscendingOrder);
		if (ModsRootItem)
			ModsRootItem->sortChildren(0, Qt::AscendingOrder);
		if (MapsRootItem)
			MapsRootItem->setExpanded(Settings.value(SectionSettingKey("Maps") + "/Expanded", true).toBool());
		if (ModsRootItem)
			ModsRootItem->setExpanded(Settings.value(SectionSettingKey("Mods") + "/Expanded", true).toBool());
	}

	if (ActiveTab != "all")
		mFileListWidget->sortItems(0, Qt::AscendingOrder);

	UpdateCategorySummary(Lookup, Recents, Favorites);
	UpdateQuickLaunchVisibility();
	UpdateBuildActionButtons();
	AppendStartupTrace(QString("PopulateFileList:done items=%1").arg(mFileListWidget ? mFileListWidget->topLevelItemCount() : -1));
}

void mlMainWindow::OnEditReadyForPublish()
{
	if (mBuildThread)
		return;

	bool HasMapSelection = false;
	QList<QTreeWidgetItem*> CheckedItems = CollectTargetItems(&HasMapSelection);
	CheckedItems.clear();
	std::function<void(QTreeWidgetItem*)> CollectCheckedOnly = [&](QTreeWidgetItem* Parent)
	{
		for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = Parent->child(ChildIdx);
			if (ItemCheckState(Child) == Qt::Checked && Child->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
			{
				CheckedItems.append(Child);
				HasMapSelection |= (Child->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP);
			}
			CollectCheckedOnly(Child);
		}
	};
	CollectCheckedOnly(mFileListWidget->invisibleRootItem());
	if (CheckedItems.isEmpty())
	{
		QMessageBox::critical(this, "Publish Check", "No items are checked. Check at least one map or mod before using Publish Check.");
		return;
	}

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Publish Check");
	Dialog.resize(860, 320);
	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QLabel* Intro = new QLabel("Prepare checked maps/mods before publishing. This pass is best used right before a Workshop upload.");
	Intro->setWordWrap(true);
	Layout->addWidget(Intro);

	QLabel* Checklist = new QLabel("Recommended before upload:\n- Make sure your thumbnail follows the proper guidelines, check the Upload and click the ? next to the thumbnail");
	Checklist->setWordWrap(true);
	Checklist->setObjectName("InfoBanner");
	Layout->addWidget(Checklist);

	QCheckBox* RemoveXPaks = new QCheckBox("Remove XPaks");
	RemoveXPaks->setChecked(true);
	RemoveXPaks->setToolTip("Deletes generated .xpak files under the checked map/mod folders.");
	Layout->addWidget(RemoveXPaks);

	QCheckBox* CompileAllLanguages = new QCheckBox("Compile all languages");
	CompileAllLanguages->setChecked(true);
	CompileAllLanguages->setToolTip("Links the checked content for every supported language.");
	Layout->addWidget(CompileAllLanguages);

	QCheckBox* BuildLights = NULL;
	if (HasMapSelection)
	{
		BuildLights = new QCheckBox("Build lights");
		BuildLights->setChecked(true);
		BuildLights->setToolTip("Runs the lighting pass for checked maps before linking.");
		Layout->addWidget(BuildLights);
	}

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	Layout->addWidget(ButtonBox);
	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return;

	if (RemoveXPaks->isChecked())
	{
		for (QTreeWidgetItem* Item : CheckedItems)
		{
			const QString Folder = (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
				? QString("%1/usermaps/%2").arg(mGamePath, GetItemEntryName(Item))
				: QString("%1/mods/%2").arg(mGamePath, GetItemContainerName(Item));
			QDirIterator It(Folder, QStringList() << "*.xpak", QDir::Files, QDirIterator::Subdirectories);
			while (It.hasNext())
				QFile(It.next()).remove();
		}
	}

	if (!CompileAllLanguages->isChecked())
		return;

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));
	QStringList LanguageArgs;
	for (int LanguageIdx = 0; LanguageIdx < ARRAYSIZE(gLanguages); LanguageIdx++)
		LanguageArgs << "-language" << gLanguages[LanguageIdx];

	for (QTreeWidgetItem* Item : CheckedItems)
	{
		TouchRecentEntry(RecentEntryForItem(Item->data(0, Qt::UserRole).toInt(), GetItemContainerName(Item), GetItemEntryName(Item)));

		if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
		{
			const QString MapName = GetItemEntryName(Item);
			QStringList CompileArgs;
			CompileArgs << "-platform" << "pc" << "-navmesh" << "-navvolume";
			CompileArgs << "-loadFrom" << QString("%1\\map_source\\%2\\%3.map").arg(mGamePath, MapName.left(2), MapName);
			CompileArgs << QString("%1\\share\\raw\\maps\\%2\\%3.d3dbsp").arg(mGamePath, MapName.left(2), MapName);
			Commands.append(QPair<QString, QStringList>(QString("%1\\bin\\cod2map64.exe").arg(mToolsPath), CompileArgs));

			if (BuildLights && BuildLights->isChecked())
			{
				QStringList LightArgs;
				LightArgs << "-ledSilent";
				switch (mLightQualityWidget->currentIndex())
				{
				case 0:
					LightArgs << "+low";
					break;
				default:
					LightArgs << "+medium";
					break;
				case 2:
					LightArgs << "+high";
					break;
				}
				LightArgs << "+localprobes" << "+forceclean" << "+recompute" << QString("%1/map_source/%2/%3.map").arg(mGamePath, MapName.left(2), MapName);
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath), LightArgs));
			}

			Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-modsource" << MapName));
		}
		else
		{
			const QString ModName = GetItemContainerName(Item);
			if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MOD_GROUP)
			{
				for (const QString& ZoneName : ModZoneNames(ModName))
					Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ModName << "-modsource" << ZoneName));
			}
			else
			{
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ModName << "-modsource" << GetItemEntryName(Item)));
			}
		}
	}

	SetActiveBuildButton(BuildAllLanguages);
	StartBuildThread(Commands);
}

void mlMainWindow::ContextMenuRequested(const QPoint& Position)
{
	if (!mFileListWidget)
		return;

	QTreeWidgetItem* Item = mFileListWidget->itemAt(Position);
	if (!Item)
		Item = mFileListWidget->currentItem();
	if (!Item)
		return;

	const int ItemTypeId = Item->data(0, Qt::UserRole).toInt();
	QString ItemType = (ItemTypeId == ML_ITEM_MAP) ? "Map" : "Mod";

	if (ItemTypeId == ML_ITEM_UNKNOWN)
		return;

	QIcon GameIcon(":/resources/BlackOps3.png");

	QMenu* Menu = new QMenu;
	Menu->setStyleSheet(
		"QMenu { padding: 6px; }"
		"#DangerMenuSection { padding-top: 4px; }"
		"#DangerMenuButton {"
		" background: transparent; color: #ff7b7b; border: 0;"
		" padding: 4px 8px; text-align: left; font-weight: 700; }"
		"#DangerMenuButton:hover { background: #4a2020; border: 0; color: #ff9f66; }"
		"#DangerMenuButton:pressed { background: #351616; border: 0; color: #ffb0b0; }");
		QAction* RunAction = Menu->addAction(GameIcon, QString("Run %1").arg(ItemType), this, SLOT(OnRunMapOrMod()));
		RunAction->setEnabled(ItemCheckState(Item) == Qt::Checked);
	Menu->addSeparator();
	QAction* InformationAction = Menu->addAction(style()->standardIcon(QStyle::SP_FileDialogInfoView), "Information");
	connect(InformationAction, &QAction::triggered, this, [=]() { ShowItemInformationDialog(Item); });
	Menu->addSeparator();

	if (ItemTypeId == ML_ITEM_MAP)
		Menu->addAction(mActionFileLevelEditor);

	if (ItemTypeId == ML_ITEM_MAP || ItemTypeId == ML_ITEM_MOD)
		Menu->addAction("Edit Zone File", this, SLOT(OnOpenZoneFile()));
	QAction* PublishAction = Menu->addAction(QString("Publish %1").arg(ItemType));
	connect(PublishAction, &QAction::triggered, this, [=]()
	{
		std::function<void(QTreeWidgetItem*)> ClearCheckedItems = [&](QTreeWidgetItem* Parent)
		{
			for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
			{
				QTreeWidgetItem* Child = Parent->child(ChildIdx);
				if (Child != Item && Child->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN && ItemCheckState(Child) == Qt::Checked)
					SetTreeItemChecked(Child, Qt::Unchecked, Child->childCount() > 0);
				ClearCheckedItems(Child);
			}
		};
		ClearCheckedItems(mFileListWidget->invisibleRootItem());
		SetTreeItemChecked(Item, Qt::Checked, Item->childCount() > 0);
		mFileListWidget->setCurrentItem(Item);
		OnEditPublish();
	});
	Menu->addAction(QString("Open %1 Folder").arg(ItemType), this, SLOT(OnOpenModRootFolder()));
	Menu->addAction(QString("Rename %1...").arg(ItemType), this, SLOT(OnRenameItem()));
	Menu->addAction(QString("Analyze %1...").arg(ItemType), this, SLOT(OnAnalyzeItem()));
	Menu->addAction(IsFavoriteEntry(GetItemFavoriteKey(Item)) ? "Remove Favorite" : "Add Favorite", this, SLOT(OnToggleFavorite()));
	QAction* NotesAction = Menu->addAction("Notes...");
	connect(NotesAction, &QAction::triggered, this, [=]() { EditNotesForItem(Item); });
	QAction* EditWorkshopJsonAction = Menu->addAction("Edit workshop.json...");
	connect(EditWorkshopJsonAction, &QAction::triggered, this, [=]()
	{
		const QString ItemFolder = (ItemTypeId == ML_ITEM_MAP)
			? QString("%1/usermaps/%2/zone").arg(mGamePath, GetItemEntryName(Item))
			: QString("%1/mods/%2/zone").arg(mGamePath, GetItemContainerName(Item));
		EditJsonTextDialog(this, QDir::cleanPath(QString("%1/workshop.json").arg(ItemFolder)), "Edit workshop.json");
	});
	QAction* WorkshopVersionsAction = Menu->addAction("Workshop Versions...");
	connect(WorkshopVersionsAction, &QAction::triggered, this, [=]() { ShowWorkshopVersionsDialog(Item); });
	Menu->addAction("Clean XPaks", this, SLOT(OnCleanXPaks()));
	Menu->addSeparator();

	QWidgetAction* DeleteAction = new QWidgetAction(Menu);
	QWidget* DeleteWidget = new QWidget(Menu);
	DeleteWidget->setObjectName("DangerMenuSection");
	DeleteWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	QHBoxLayout* DeleteLayout = new QHBoxLayout(DeleteWidget);
	DeleteLayout->setContentsMargins(0, 0, 0, 0);
	DeleteLayout->setSpacing(0);
	QPushButton* DeleteButton = new QPushButton(DeleteWidget);
	DeleteButton->setFlat(true);
	DeleteButton->setIcon(Menu->style()->standardIcon(QStyle::SP_MessageBoxWarning));
	DeleteButton->setIconSize(QSize(13, 13));
	DeleteButton->setText(QString("Delete %1").arg(ItemType));
	DeleteButton->setObjectName("DangerMenuButton");
	DeleteButton->setCursor(Qt::PointingHandCursor);
	DeleteButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	DeleteButton->setMinimumHeight(24);
	DeleteLayout->addWidget(DeleteButton);
	connect(DeleteButton, &QPushButton::clicked, Menu, [=]()
	{
		Menu->close();
		OnDelete();
	});
	DeleteAction->setDefaultWidget(DeleteWidget);
	Menu->addAction(DeleteAction);

	mFileListWidget->setCurrentItem(Item);
	Menu->exec(mFileListWidget->viewport()->mapToGlobal(Position));
}

void mlMainWindow::OnFileAssetEditor()
{
	QProcess* Process = new QProcess();
	TrackProcessLifetime(Process, "Stats/AssetEditorLaunches", "Stats/AssetEditorSeconds");
	Process->start(QString("%1/bin/AssetEditor_modtools.exe").arg(mToolsPath), QStringList());
}

void mlMainWindow::OnFileLevelEditor()
{
	auto LaunchRadiant = [this](const QString& MapName, const QString& FavoriteKey)
	{
		QProcess* Process = new QProcess();
		if (!FavoriteKey.isEmpty())
		{
			TouchRecentEntry(FavoriteKey);
			TrackProcessLifetime(Process, "Stats/RadiantLaunches", "Stats/RadiantSeconds", "Stats/ItemsRadiantSeconds", FavoriteKey);
			Process->start(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath), QStringList() << QString("%1/map_source/%2/%3.map").arg(mGamePath, MapName.left(2), MapName));
		}
		else
		{
			TrackProcessLifetime(Process, "Stats/RadiantLaunches", "Stats/RadiantSeconds", "Stats/ItemsRadiantSeconds", "radiant:empty");
			Process->start(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath));
		}
	};

	QTreeWidgetItem* Item = ActiveFileItem();
	if (!Item || Item->data(0, Qt::UserRole).toInt() != ML_ITEM_MAP)
	{
		QMessageBox Prompt(this);
		Prompt.setIcon(QMessageBox::Warning);
		Prompt.setWindowTitle("Open Radiant");
		Prompt.setText("No map selected.");
		Prompt.setInformativeText("Select a map first, or open an empty Radiant session.");
		QPushButton* EmptyButton = Prompt.addButton("Empty Radiant Map", QMessageBox::AcceptRole);
		Prompt.addButton("Cancel", QMessageBox::RejectRole);
		Prompt.exec();
		if (Prompt.clickedButton() == EmptyButton)
			LaunchRadiant(QString(), QString());
		return;
	}

	const QString MapName = GetItemEntryName(Item);
	const QString FavoriteKey = GetItemFavoriteKey(Item);
	if (MapName.isEmpty())
		return;
	LaunchRadiant(MapName, FavoriteKey);
}

void mlMainWindow::OnFileExport2Bin()
{
	if (mExport2BinGUIWidget == NULL)
	{
		InitExport2BinGUI();
		mExport2BinGUIWidget->hide(); // Ensure the window is hidden (just in case)
	}

	mExport2BinGUIWidget->isVisible() ? mExport2BinGUIWidget->hide() : mExport2BinGUIWidget->show();
}

void mlMainWindow::OnFileNew()
{
	QDir TemplatesFolder(QString("%1/rex/templates").arg(mToolsPath));
	QStringList Templates = TemplatesFolder.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

	if (Templates.isEmpty())
	{
		QMessageBox::information(this, "Error", "Could not find any map templates.");
		return;
	}

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("New Map or Mod");
	Dialog.resize(560, 300);
	Dialog.setMinimumSize(500, 260);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	Layout->setContentsMargins(14, 8, 14, 10);
	Layout->setSpacing(8);
	QLabel* IntroLabel = new QLabel("Create a new map or mod.", &Dialog);
	IntroLabel->setWordWrap(true);
	QFont IntroFont = IntroLabel->font();
	IntroFont.setPointSize(IntroFont.pointSize() + 2);
	IntroFont.setBold(true);
	IntroLabel->setFont(IntroFont);
	Layout->addWidget(IntroLabel);

	QLabel* NamingLabel = new QLabel("Naming scheme: multiplayer maps use mp_*, zombie maps use zm_*, and mods use your mod folder name.", &Dialog);
	NamingLabel->setWordWrap(true);
	Layout->addWidget(NamingLabel);

	QFormLayout* FormLayout = new QFormLayout();
	FormLayout->setContentsMargins(0, 0, 0, 0);
	FormLayout->setHorizontalSpacing(10);
	FormLayout->setVerticalSpacing(8);
	Layout->addLayout(FormLayout);

	QLineEdit* NameWidget = new QLineEdit();
	NameWidget->setValidator(new QRegularExpressionValidator(QRegularExpression("[a-zA-Z0-9_]*"), this));
	NameWidget->setPlaceholderText("zm_example");
	FormLayout->addRow("Name:", NameWidget);

	QLineEdit* DisplayNameWidget = new QLineEdit(&Dialog);
	DisplayNameWidget->setPlaceholderText("Leviathan");
	FormLayout->addRow("Display name:", DisplayNameWidget);

	QLineEdit* ColorEdit = new QLineEdit(&Dialog);
	ColorEdit->setPlaceholderText("#ff8a2a");
	QPushButton* ColorBrowseButton = new QPushButton(&Dialog);
	ColorBrowseButton->setCursor(Qt::PointingHandCursor);
	ColorBrowseButton->setFixedSize(26, 26);
	ColorBrowseButton->setToolTip("Pick selection color");
	QToolButton* ColorHelpButton = new QToolButton(&Dialog);
	ColorHelpButton->setText("?");
	ColorHelpButton->setToolTip("Colors only change how this map or mod appears inside the launcher list.");
	QPushButton* ClearColorButton = new QPushButton("Clear", &Dialog);
	QWidget* ColorRowWidget = new QWidget(&Dialog);
	QHBoxLayout* ColorRowLayout = new QHBoxLayout(ColorRowWidget);
	ColorRowLayout->setContentsMargins(0, 0, 0, 0);
	ColorRowLayout->setSpacing(6);
	ColorRowLayout->addWidget(ColorEdit, 1);
	ColorRowLayout->addWidget(ColorBrowseButton);
	ColorRowLayout->addWidget(ColorHelpButton);
	ColorRowLayout->addWidget(ClearColorButton);
	FormLayout->addRow("Selection color:", ColorRowWidget);

	QComboBox* TemplateWidget = new QComboBox();
	for (const QString& TemplateName : Templates)
	{
		QString DisplayLabel = TemplateName;
		if (TemplateName.compare("ZM Mod Level", Qt::CaseInsensitive) == 0)
			DisplayLabel = "Zombie Map (ZM Mod Level)";
		else if (TemplateName.compare("MP Mod Level", Qt::CaseInsensitive) == 0)
			DisplayLabel = "Multiplayer Map (MP Mod Level)";
		else if (TemplateName.compare("mod", Qt::CaseInsensitive) == 0)
			DisplayLabel = "Mod (mod)";
		TemplateWidget->addItem(DisplayLabel, TemplateName);
	}
	const int DefaultTemplateIndex = TemplateWidget->findData("ZM Mod Level");
	TemplateWidget->setCurrentIndex(DefaultTemplateIndex >= 0 ? DefaultTemplateIndex : 0);
	FormLayout->addRow("Template:", TemplateWidget);
	auto RefreshAutoFields = [=]()
	{
		const QString Name = NameWidget->text().trimmed();
		const QString RawTemplateName = TemplateWidget->currentData().toString();
		const bool IsMapTemplate = RawTemplateName.compare("ZM Mod Level", Qt::CaseInsensitive) == 0
			|| RawTemplateName.compare("MP Mod Level", Qt::CaseInsensitive) == 0;
		if (ColorEdit->text().trimmed().isEmpty())
			ColorEdit->setPlaceholderText(Name.isEmpty() ? "Auto color" : DefaultDisplayColorForEntryName(Name, IsMapTemplate));
		ColorBrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(
			NormalizedStoredColor(ColorEdit->text()).isEmpty()
				? (Name.isEmpty() ? QString("#444444") : DefaultDisplayColorForEntryName(Name, IsMapTemplate))
				: NormalizedStoredColor(ColorEdit->text())));
	};
	connect(NameWidget, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshAutoFields(); });
	connect(TemplateWidget, &QComboBox::currentTextChanged, &Dialog, [=](const QString&) { RefreshAutoFields(); });
	connect(ColorEdit, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshAutoFields(); });
	connect(ColorBrowseButton, &QPushButton::clicked, &Dialog, [&, ColorEdit]()
	{
		const QColor Picked = QColorDialog::getColor(QColor(ColorEdit->text()), &Dialog, "Select Selection Color");
		if (Picked.isValid())
			ColorEdit->setText(Picked.name(QColor::HexRgb));
	});
	connect(ClearColorButton, &QPushButton::clicked, &Dialog, [=]() { ColorEdit->clear(); });
	connect(ColorHelpButton, &QToolButton::clicked, &Dialog, [&, this]()
	{
		QMessageBox::information(&Dialog, "Selection Color", "Use a custom color to make a favorite map or mod stand out in the launcher list. This only changes the launcher UI, not the in-game name.");
	});
	RefreshAutoFields();

	QHBoxLayout* ButtonRow = new QHBoxLayout();
	QPushButton* CancelButton = new QPushButton("Cancel", &Dialog);
	ButtonRow->addWidget(CancelButton);
	ButtonRow->addStretch();
	QPushButton* CreateButton = new QPushButton("Create", &Dialog);
	ButtonRow->addWidget(CreateButton);
	Layout->addLayout(ButtonRow);
	int RequestedCreationMode = -1;

	const auto TemplateIsMap = [TemplateWidget]()
	{
		const QString TemplateName = TemplateWidget->currentData().toString();
		return TemplateName.compare("mod", Qt::CaseInsensitive) != 0;
	};
	connect(CreateButton, &QPushButton::clicked, &Dialog, [&]()
	{
		RequestedCreationMode = TemplateIsMap() ? 1 : 0;
		Dialog.accept();
	});
	connect(CancelButton, &QPushButton::clicked, &Dialog, [&]()
	{
		Dialog.reject();
	});

	if (Dialog.exec() != QDialog::Accepted)
		return;

	const QString Template = TemplateWidget->currentData().toString();
	const bool IsMapTemplate = Template.compare("mod", Qt::CaseInsensitive) != 0;
	const bool CreateMap = RequestedCreationMode == 1;
	if (RequestedCreationMode < 0)
		return;

	QString Name = NameWidget->text();

	if (Name.isEmpty())
	{
		QMessageBox::information(this, "Error", "Map name cannot be empty.");
		return;
	}

	if (mShippedMapList.contains(Name, Qt::CaseInsensitive))
	{
		QMessageBox::information(this, "Error", "Map name cannot be the same as a built-in map.");
		return;
	}

	QByteArray MapName = NameWidget->text().toLatin1().toLower();
	QString Output;
	const QString CustomDisplayName = DisplayNameWidget->text().trimmed();
	const QString CustomDisplayColor = NormalizedStoredColor(ColorEdit->text());

	if (CreateMap && ((Template == "MP Mod Level" && !MapName.startsWith("mp_")) || (Template == "ZM Mod Level" && !MapName.startsWith("zm_"))))
	{
		QMessageBox::information(this, "Error", "Map name must start with 'mp_' or 'zm_'.");
		return;
	}

	std::function<bool(const QString&, const QString&)> RecursiveCopy=[&](const QString& SourcePath, const QString& DestPath) -> bool
	{
		QDir Dir(SourcePath);
		if (!Dir.exists())
			return false;

		foreach (QString DirEntry, Dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
		{
			QString NewPath = QString(DestPath + QDir::separator() + DirEntry).replace(QString("template"), MapName);
			Dir.mkpath(NewPath);
			if (!RecursiveCopy(SourcePath + QDir::separator() + DirEntry, NewPath))
				return false;
		}

		foreach (QString DirEntry, Dir.entryList(QDir::Files))
		{
			QFile SourceFile(SourcePath + QDir::separator() + DirEntry);
			QString DestFileName = QString(DestPath + QDir::separator() + DirEntry).replace(QString("template"), MapName);
			QFile DestFile(DestFileName);

			if (!SourceFile.open(QFile::ReadOnly) || !DestFile.open(QFile::WriteOnly))
				return false;

			while (!SourceFile.atEnd())
			{
				QByteArray Line = SourceFile.readLine();

				if (Line.contains("guid"))
				{
					QString LineString(Line);
					LineString.replace(QRegularExpression("guid \"\\{(.*)\\}\""), QString("guid \"%1\"").arg(QUuid::createUuid().toString()));
					Line = LineString.toLatin1();
				}
				else
					Line.replace("template", MapName);

				DestFile.write(Line);
			}

			Output += DestFileName + "\n";
		}

		return true;
	};

	if (RecursiveCopy(TemplatesFolder.absolutePath() + QDir::separator() + Templates[TemplateWidget->currentIndex()], QDir::cleanPath(mGamePath)))
	{
		const bool CreatedMap = QDir(QDir::cleanPath(QString("%1/usermaps/%2").arg(mGamePath, Name))).exists();
		const bool CreatedMod = QDir(QDir::cleanPath(QString("%1/mods/%2").arg(mGamePath, Name))).exists();
		if (CreatedMap || CreatedMod)
		{
			const QString FavoriteKey = CreatedMap ? QString("map:%1").arg(Name.toLower()) : QString("mod:%1").arg(Name.toLower());
			if (DisplayNameForEntry(FavoriteKey).isEmpty())
				SetDisplayNameForEntry(FavoriteKey, CustomDisplayName.isEmpty() ? DefaultDisplayNameForEntryName(Name) : CustomDisplayName);
			if (DisplayColorForEntry(FavoriteKey).isEmpty())
				SetDisplayColorForEntry(FavoriteKey, CustomDisplayColor.isEmpty() ? DefaultDisplayColorForEntryName(Name, CreatedMap) : CustomDisplayColor);
		}
		PopulateFileList();

		QMessageBox::information(this, "New Map Created", QString("Files created:\n") + Output);
	}
	else
		QMessageBox::information(this, "Error", "Error creating map files.");
}

void mlMainWindow::OnEditBuild()
{
	StartBuild(BuildAllLanguages);
}

void mlMainWindow::OnEditBuildAllLanguages()
{
	StartBuild(BuildEnglishOnly);
}

void mlMainWindow::OnSetModernTheme()
{
	ApplyThemeProfile("original-updated");
	UpdateTheme();
	PopulateFileList();
}

void mlMainWindow::OnSetDarkModernTheme()
{
	ApplyThemeProfile("dark-modern");
	UpdateTheme();
	PopulateFileList();
}

void mlMainWindow::OnSetClassicTheme()
{
	ApplyThemeProfile("original-classic");
	UpdateTheme();
	PopulateFileList();
}
void mlMainWindow::UpdateThemeMenuChecks()
{
	if (mActionThemeModern)
	{
		mActionThemeModern->blockSignals(true);
		mActionThemeModern->setChecked(mThemeMode == ThemeOriginalUpdated);
		mActionThemeModern->blockSignals(false);
	}
	if (mActionThemeDarkModern)
	{
		mActionThemeDarkModern->blockSignals(true);
		mActionThemeDarkModern->setChecked(mThemeMode == ThemeDarkModern);
		mActionThemeDarkModern->blockSignals(false);
	}
	if (mActionThemeClassic)
	{
		mActionThemeClassic->blockSignals(true);
		mActionThemeClassic->setChecked(mThemeMode == ThemeOriginalClassic);
		mActionThemeClassic->blockSignals(false);
	}
}

void mlMainWindow::StartBuild(BuildLanguageMode Mode)
{
	QPushButton* TriggerButton = (Mode == BuildAllLanguages) ? mBuildButton : mBuildAllLanguagesButton;
	const bool RunOnlyMode = mRunEnabledWidget && mRunEnabledWidget->isChecked()
		&& mCompileEnabledWidget && !mCompileEnabledWidget->isChecked()
		&& mLightEnabledWidget && !mLightEnabledWidget->isChecked()
		&& mLinkEnabledWidget && !mLinkEnabledWidget->isChecked();
	const bool ExplicitCompile = mCompileEnabledWidget && mCompileEnabledWidget->isChecked();
	const bool ExplicitLight = mLightEnabledWidget && mLightEnabledWidget->isChecked();
	const bool ExplicitLink = mLinkEnabledWidget && mLinkEnabledWidget->isChecked();
	const bool ExplicitRun = mRunEnabledWidget && mRunEnabledWidget->isChecked();

	if (mBuildThread)
	{
		if (mActiveBuildButton == TriggerButton)
			mBuildThread->Cancel();
		return;
	}

	QList<QPair<QString, QStringList>> Commands;
	bool UpdateAdded = false;
	bool HasTargetToBuild = false;
	auto AddUpdateDBCommand = [&]()
	{
		if (!UpdateAdded)
		{
			Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));
			UpdateAdded = true;
		}
	};

	QList<QTreeWidgetItem*> CheckedItems = CollectTargetItems();
	if (CheckedItems.isEmpty())
		{
			QMessageBox::warning(this, "Build", "Please select at least one file from the list.");
		return;
	}
	QString LastMap, LastMod;

	QStringList LanguageArgs;

	if (Mode == BuildEnglishOnly)
	{
		LanguageArgs << "-language" << "english";
	}
	else if (Mode == BuildAllLanguages || mBuildLanguage == "All")
	{
		for (const QString& Language : gLanguages)
			LanguageArgs << "-language" << Language;
	}
	else
		LanguageArgs << "-language" << mBuildLanguage;

	for (QTreeWidgetItem* Item : CheckedItems)
	{
		TouchRecentEntry(RecentEntryForItem(Item->data(0, Qt::UserRole).toInt(), GetItemContainerName(Item), GetItemEntryName(Item)));

		if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
		{
			QString MapName = GetItemEntryName(Item);
			HasTargetToBuild = true;
			const bool DefaultBuildMode = !ExplicitCompile && !ExplicitLight && !ExplicitLink && !ExplicitRun;

			if (ExplicitCompile || DefaultBuildMode)
			{
				AddUpdateDBCommand();

				QStringList Args;
				Args << "-platform" << "pc";

				if (mCompileModeWidget->currentIndex() == 0)
					Args << "-onlyents";
				else
					Args << "-navmesh" << "-navvolume";

				Args << "-loadFrom" << QString("%1\\map_source\\%2\\%3.map").arg(mGamePath, MapName.left(2), MapName);
				Args << QString("%1\\share\\raw\\maps\\%2\\%3.d3dbsp").arg(mGamePath, MapName.left(2), MapName);

				Commands.append(QPair<QString, QStringList>(QString("%1\\bin\\cod2map64.exe").arg(mToolsPath), Args));
			}

			if (ExplicitLight)
			{
				AddUpdateDBCommand();

				QStringList Args;
				Args << "-ledSilent";

				switch (mLightQualityWidget->currentIndex())
				{
				case 0:
					Args << "+low";
					break;
				default:
					Args << "+medium";
					break;

				case 2:
					Args << "+high";
					break;
				}

				Args << "+localprobes" << "+forceclean" << "+recompute" << QString("%1/map_source/%2/%3.map").arg(mGamePath, MapName.left(2), MapName);
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath), Args));
			}

			if (ExplicitLink || DefaultBuildMode)
			{
				AddUpdateDBCommand();

				Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-modsource" << MapName));
			}

			LastMap = MapName;
		}
		else
		{
			QString ModName = GetItemContainerName(Item);
			const int ItemType = Item->data(0, Qt::UserRole).toInt();
			HasTargetToBuild = true;
			const bool DefaultBuildMode = !ExplicitCompile && !ExplicitLight && !ExplicitLink && !ExplicitRun;

			if (ExplicitLink || DefaultBuildMode)
			{
				AddUpdateDBCommand();

				if (ItemType == ML_ITEM_MOD_GROUP)
				{
					for (const QString& ZoneName : ModZoneNames(ModName))
						Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ModName << "-modsource" << ZoneName));
				}
				else
				{
					QString ZoneName = GetItemEntryName(Item);
					Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ModName << "-modsource" << ZoneName));
				}
			}

			LastMod = ModName;
		}
	}

	if (ExplicitRun && (!LastMod.isEmpty() || !LastMap.isEmpty()))
	{
		QueueGameStatsForItem(LastMap.isEmpty() ? QString("mod:%1").arg(LastMod.toLower()) : QString("map:%1").arg(LastMap.toLower()));
		Commands.append(CreateGameLaunchCommand(LastMod.isEmpty() ? LastMap : LastMod, LastMap));
	}

	if (Commands.size() == 0 && !UpdateAdded && !HasTargetToBuild)
	{
		QMessageBox::information(this, "No Tasks", "Please selected at least one file from the list and one action to be performed.");
		return;
	}

	SetActiveBuildButton(Mode);
	StartBuildThread(Commands);
}

void mlMainWindow::OnEditPublish()
{
	std::function<QTreeWidgetItem* (QTreeWidgetItem*)> SearchCheckedItem=[&](QTreeWidgetItem* ParentItem) -> QTreeWidgetItem*
	{
		for (int ChildIdx = 0; ChildIdx < ParentItem->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = ParentItem->child(ChildIdx);
			if (ItemCheckState(Child) == Qt::Checked)
				return Child;

			QTreeWidgetItem* Checked = SearchCheckedItem(Child);
			if (Checked)
				return Checked;
		}

		return nullptr;
	};

	QTreeWidgetItem* Item = SearchCheckedItem(mFileListWidget->invisibleRootItem());
	if (!Item)
	{
		QMessageBox::warning(this, "Error", "No maps or mods checked.");
		return;
	}

	QString Folder;
	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		Folder = "usermaps/" + GetItemEntryName(Item);
		mType = "map";
		mFolderName = GetItemEntryName(Item);
	}
	else
	{
		Folder = "mods/" + GetItemContainerName(Item);
		mType = "mod";
		mFolderName = GetItemContainerName(Item);
	}

	mWorkshopFolder = QString("%1/%2/zone").arg(mGamePath, Folder);
	QFile File(mWorkshopFolder + "/workshop.json");

	if (!QFileInfo(mWorkshopFolder).isDir())
	{
		QMessageBox::information(this, "Error", QString("The folder '%1' does not exist.").arg(mWorkshopFolder));
		return;
	}

	mFileId = 0;
	mTitle.clear();
	mBriefingDescription.clear();
	mSteamDescription.clear();
	mThumbnail.clear();
	mTags.clear();
	mPostUploadSteamSyncPending = false;

	if (File.open(QIODevice::ReadOnly))
	{
		QJsonDocument Document = QJsonDocument::fromJson(File.readAll());
		QJsonObject Root = Document.object();

		mFileId = Root["PublisherID"].toString().toULongLong();
		mTitle = Root["Title"].toString();
		mBriefingDescription = Root.value("Description").toString(Root.value("BriefingDescription").toString());
		mSteamDescription = Root.value("SteamDescription").toString(mBriefingDescription);
		mThumbnail = Root["Thumbnail"].toString();
		mTags = Root["Tags"].toString().split(',');
	}

	if (mFileId)
	{
		SteamAPICall_t SteamAPICall = SteamUGC()->RequestUGCDetails(mFileId, 10);
		mSteamCallResultRequestDetails.Set(SteamAPICall, this, &mlMainWindow::OnUGCRequestUGCDetails);
	}
	else
		ShowPublishDialog();
}

void mlMainWindow::OnUGCRequestUGCDetails(SteamUGCRequestUGCDetailsResult_t* RequestDetailsResult, bool IOFailure)
{
	if (IOFailure || RequestDetailsResult->m_details.m_eResult != k_EResultOK)
	{
		QMessageBox::warning(this, "Error", "Error retrieving item data from the Steam Workshop.");
		return;
	}

	SteamUGCDetails_t* Details = &RequestDetailsResult->m_details;

	if (mTitle.trimmed().isEmpty())
		mTitle = Details->m_rgchTitle;
	if (mBriefingDescription.trimmed().isEmpty())
		mBriefingDescription = Details->m_rgchDescription;
	if (mSteamDescription.trimmed().isEmpty())
		mSteamDescription = Details->m_rgchDescription;
	mTags = QString(Details->m_rgchTags).split(',');

	ShowPublishDialog();
}

void mlMainWindow::ShowPublishDialog()
{
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Publish Mod");
	Dialog.resize(1120, 780);
	Dialog.setMinimumSize(1020, 720);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	Layout->setSpacing(14);
	const QString WorkshopJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(mWorkshopFolder));
	const QString WorkshopFolderPath = mWorkshopFolder;

	const QString ContentRoot = QFileInfo(mWorkshopFolder).dir().absolutePath();
	const QStringList BuiltLanguages = DetectBuiltLanguages(ContentRoot);
	if (HasOnlyEnglishBuild(ContentRoot))
	{
		QLabel* WarningLabel = new QLabel("Warning: this item currently appears to have only English localized files built. Uploading now may ship English-only Workshop content.");
		WarningLabel->setWordWrap(true);
		WarningLabel->setObjectName("WarningBanner");
		Layout->addWidget(WarningLabel);
	}
	else if (BuiltLanguages.isEmpty())
	{
		QLabel* WarningLabel = new QLabel("Warning: no localized build folders were detected for this item. Verify your latest link output before uploading.");
		WarningLabel->setWordWrap(true);
		WarningLabel->setObjectName("WarningBanner");
		Layout->addWidget(WarningLabel);
	}

	const bool AllLanguagesDetected = (BuiltLanguages.count() == ARRAYSIZE(gLanguages));
	QLabel* SummaryLabel = new QLabel(QString("Detected languages: %1").arg(BuiltLanguages.isEmpty() ? "none" : BuiltLanguages.join(", ")));
	SummaryLabel->setWordWrap(true);
	SummaryLabel->setObjectName(AllLanguagesDetected ? "SuccessBanner" : "WarningBanner");
	Layout->addWidget(SummaryLabel);

	QSplitter* Splitter = new QSplitter(&Dialog);
	Layout->addWidget(Splitter, 1);

	QWidget* EditorPanel = new QWidget(Splitter);
	QVBoxLayout* EditorLayout = new QVBoxLayout(EditorPanel);
	EditorLayout->setContentsMargins(0, 0, 0, 0);
	EditorLayout->setSpacing(12);

	QHBoxLayout* VersionLayout = new QHBoxLayout();
	VersionLayout->addWidget(new QLabel("Workshop Versions:"));
	QPushButton* VersionManagerButton = new QPushButton("Open Version Manager");
	VersionLayout->addWidget(VersionManagerButton);
	VersionLayout->addStretch(1);
	EditorLayout->addLayout(VersionLayout);

	QFormLayout* FormLayout = new QFormLayout();
	FormLayout->setSpacing(12);
	EditorLayout->addLayout(FormLayout);

	QLineEdit* TitleWidget = new QLineEdit();
	TitleWidget->setText(mTitle);
	QToolButton* TitleHelpButton = new QToolButton();
	TitleHelpButton->setText("?");
	TitleHelpButton->setToolTip("Title tips");
	QWidget* TitleRowWidget = new QWidget(&Dialog);
	QHBoxLayout* TitleRowLayout = new QHBoxLayout(TitleRowWidget);
	TitleRowLayout->setContentsMargins(0, 0, 0, 0);
	TitleRowLayout->setSpacing(6);
	TitleRowLayout->addWidget(TitleWidget, 1);
	TitleRowLayout->addWidget(TitleHelpButton, 0, Qt::AlignTop);
	FormLayout->addRow("Title:", TitleRowWidget);

	QTextEdit* BriefingDescriptionWidget = new QTextEdit();
	BriefingDescriptionWidget->setAcceptRichText(false);
	BriefingDescriptionWidget->setPlainText(mBriefingDescription);
	BriefingDescriptionWidget->setMinimumHeight(120);
	BriefingDescriptionWidget->setPlaceholderText("Uploaded first. This is the in-game briefing description.");
	QToolButton* BriefingDescriptionHelpButton = new QToolButton();
	BriefingDescriptionHelpButton->setText("?");
	BriefingDescriptionHelpButton->setToolTip("Briefing description tips");
	QWidget* BriefingDescriptionRowWidget = new QWidget(&Dialog);
	QHBoxLayout* BriefingDescriptionRowLayout = new QHBoxLayout(BriefingDescriptionRowWidget);
	BriefingDescriptionRowLayout->setContentsMargins(0, 0, 0, 0);
	BriefingDescriptionRowLayout->setSpacing(6);
	BriefingDescriptionRowLayout->addWidget(BriefingDescriptionWidget, 1);
	BriefingDescriptionRowLayout->addWidget(BriefingDescriptionHelpButton, 0, Qt::AlignTop);
	FormLayout->addRow("Briefing Description:", BriefingDescriptionRowWidget);

	QTextEdit* SteamDescriptionWidget = new QTextEdit();
	SteamDescriptionWidget->setAcceptRichText(false);
	SteamDescriptionWidget->setPlainText(mSteamDescription);
	SteamDescriptionWidget->setMinimumHeight(160);
	SteamDescriptionWidget->setPlaceholderText("Automatically pushed to the Workshop after upload. Leave blank to reuse the briefing description.");
	QToolButton* SteamDescriptionHelpButton = new QToolButton();
	SteamDescriptionHelpButton->setText("?");
	SteamDescriptionHelpButton->setToolTip("Steam description tips");
	QWidget* SteamDescriptionRowWidget = new QWidget(&Dialog);
	QHBoxLayout* SteamDescriptionRowLayout = new QHBoxLayout(SteamDescriptionRowWidget);
	SteamDescriptionRowLayout->setContentsMargins(0, 0, 0, 0);
	SteamDescriptionRowLayout->setSpacing(6);
	SteamDescriptionRowLayout->addWidget(SteamDescriptionWidget, 1);
	SteamDescriptionRowLayout->addWidget(SteamDescriptionHelpButton, 0, Qt::AlignTop);
	FormLayout->addRow("Steam Description:", SteamDescriptionRowWidget);

	QLineEdit* ThumbnailEdit = new QLineEdit();
	ThumbnailEdit->setText(mThumbnail);

	QToolButton* ThumbnailButton = new QToolButton();
	ThumbnailButton->setText("...");
	QToolButton* ThumbnailHelpButton = new QToolButton();
	ThumbnailHelpButton->setText("?");
	ThumbnailHelpButton->setToolTip(
		"__**High Quality Steam Workshop Thumbnails:**__\n"
		"**File Size:** Under 1MB ( Anything over will result in launcher giving the \"steam error code\" message )\n"
		"**File Resolution:** 2048x2048 ( You can use any power of 2 as long as the file size is under 1MB )\n"
		"**File Format:** PNG or JPG ( JPG will have smaller file sizes, but comes with a slight loss in quality )\n\n"
		"__**High Quality Steam Workshop Preview Images:**__\n"
		"**File Size:** Under 2MB\n"
		"**File Resolution:** 1920 x 1080 ( Any resolution is fine as long as you are under 2MB in file size - some resolutions will be scaled by steam )\n"
		"**File Format:** PNG or JPG ( JPG will have smaller file sizes, but comes with a slight loss in quality )\n\n"
		"*Gifs are also an option, but make sure to stay under the 1 MB*");

	QHBoxLayout* ThumbnailLayout = new QHBoxLayout();
	ThumbnailLayout->setContentsMargins(0, 0, 0, 0);
	ThumbnailLayout->addWidget(ThumbnailEdit);
	ThumbnailLayout->addWidget(ThumbnailButton);
	ThumbnailLayout->addWidget(ThumbnailHelpButton);

	QWidget* ThumbnailWidget = new QWidget();
	ThumbnailWidget->setLayout(ThumbnailLayout);

	FormLayout->addRow("Thumbnail:", ThumbnailWidget);

	QWidget* TagsWidget = new QWidget(&Dialog);
	QGridLayout* TagsLayout = new QGridLayout(TagsWidget);
	TagsLayout->setContentsMargins(0, 0, 0, 0);
	TagsLayout->setHorizontalSpacing(12);
	TagsLayout->setVerticalSpacing(6);
	QList<QCheckBox*> TagCheckboxes;
	for (int TagIdx = 0; TagIdx < ARRAYSIZE(gTags); TagIdx++)
	{
		QCheckBox* TagBox = new QCheckBox(gTags[TagIdx], TagsWidget);
		TagBox->setChecked(mTags.contains(gTags[TagIdx]));
		TagsLayout->addWidget(TagBox, TagIdx / 4, TagIdx % 4);
		TagCheckboxes.append(TagBox);
	}
	QWidget* TagsRowWidget = new QWidget(&Dialog);
	QHBoxLayout* TagsRowLayout = new QHBoxLayout(TagsRowWidget);
	TagsRowLayout->setContentsMargins(0, 0, 0, 0);
	TagsRowLayout->setSpacing(6);
	TagsRowLayout->addWidget(TagsWidget, 1);
	QToolButton* TagsHelpButton = new QToolButton(TagsRowWidget);
	TagsHelpButton->setText("?");
	TagsHelpButton->setToolTip("Tag tips");
	TagsRowLayout->addWidget(TagsHelpButton, 0, Qt::AlignTop);
	FormLayout->addRow("Tags:", TagsRowWidget);

	QWidget* PreviewPanel = new QWidget(Splitter);
	QVBoxLayout* PreviewLayout = new QVBoxLayout(PreviewPanel);
	PreviewLayout->setContentsMargins(10, 0, 0, 0);
	PreviewLayout->setSpacing(10);
	QWidget* SummaryPanel = new QWidget(PreviewPanel);
	QHBoxLayout* SummaryLayout = new QHBoxLayout(SummaryPanel);
	SummaryLayout->setContentsMargins(0, 0, 0, 0);
	SummaryLayout->setSpacing(12);
	QLabel* ThumbnailPreview = new QLabel("No Preview");
	ThumbnailPreview->setObjectName("BackgroundPreview");
	ThumbnailPreview->setFixedSize(118, 118);
	ThumbnailPreview->setAlignment(Qt::AlignCenter);
	auto ResolveThumbnailPath = [=](const QString& ThumbnailValue) -> QString
	{
		if (ThumbnailValue.trimmed().isEmpty())
			return QString();
		QFileInfo ThumbInfo(ThumbnailValue);
		if (ThumbInfo.isAbsolute())
			return ThumbInfo.absoluteFilePath();
		return QDir::cleanPath(QString("%1/%2").arg(WorkshopFolderPath, ThumbnailValue));
	};
	UpdateBackgroundPreviewLabel(ThumbnailPreview, ResolveThumbnailPath(mThumbnail));
	SummaryLayout->addWidget(ThumbnailPreview, 0, Qt::AlignTop);
	QVBoxLayout* SummaryTextLayout = new QVBoxLayout();
	SummaryTextLayout->setContentsMargins(0, 0, 0, 0);
	SummaryTextLayout->setSpacing(4);
	QLabel* PreviewTitle = new QLabel("Steam Workshop Preview");
	SummaryTextLayout->addWidget(PreviewTitle);
	QLabel* PreviewTitleValue = new QLabel(StripTreyarchColorCodes(mTitle));
	PreviewTitleValue->setWordWrap(true);
	PreviewTitleValue->setStyleSheet("font-weight:700;");
	SummaryTextLayout->addWidget(PreviewTitleValue);
	QLabel* PreviewIdValue = new QLabel(QString("Workshop ID: %1").arg(mFileId ? QString::number(mFileId) : "new item"));
	PreviewIdValue->setWordWrap(true);
	SummaryTextLayout->addWidget(PreviewIdValue);
	QLabel* PreviewTagsValue = new QLabel(QString("Tags: %1").arg(mTags.isEmpty() ? "none" : mTags.join(", ")));
	PreviewTagsValue->setWordWrap(true);
	SummaryTextLayout->addWidget(PreviewTagsValue);
	SummaryTextLayout->addStretch(1);
	SummaryLayout->addLayout(SummaryTextLayout, 1);
	PreviewLayout->addWidget(SummaryPanel);
	QLabel* DescriptionPreviewTitle = new QLabel("Steam Description Preview");
	PreviewLayout->addWidget(DescriptionPreviewTitle);
	QTextBrowser* PreviewWidget = new QTextBrowser(PreviewPanel);
	PreviewWidget->setObjectName("MarkdownPreview");
	PreviewWidget->setOpenExternalLinks(true);
	PreviewWidget->document()->setDocumentMargin(8);
	PreviewWidget->setHtml(SteamMarkupToHtml(SteamDescriptionForUpload(mBriefingDescription, mSteamDescription)));
	PreviewWidget->setMinimumWidth(540);
	PreviewLayout->addWidget(PreviewWidget, 1);
	auto RefreshPreviewMeta = [=, &TagCheckboxes]()
	{
		QStringList ActiveTags;
		for (QCheckBox* TagBox : TagCheckboxes)
		{
			if (TagBox->isChecked())
				ActiveTags.append(TagBox->text());
		}
		PreviewTitleValue->setText(StripTreyarchColorCodes(TitleWidget->text()));
		PreviewIdValue->setText(QString("Workshop ID: %1").arg(mFileId ? QString::number(mFileId) : "new item"));
		PreviewTagsValue->setText(QString("Tags: %1").arg(ActiveTags.isEmpty() ? "none" : ActiveTags.join(", ")));
		PreviewWidget->setHtml(SteamMarkupToHtml(SteamDescriptionForUpload(BriefingDescriptionWidget->toPlainText(), SteamDescriptionWidget->toPlainText())));
	};
	connect(BriefingDescriptionWidget, &QTextEdit::textChanged, &Dialog, RefreshPreviewMeta);
	connect(SteamDescriptionWidget, &QTextEdit::textChanged, &Dialog, RefreshPreviewMeta);
	connect(TitleWidget, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshPreviewMeta(); });
	for (QCheckBox* TagBox : TagCheckboxes)
		connect(TagBox, &QCheckBox::toggled, &Dialog, [=](bool) { RefreshPreviewMeta(); });
	Splitter->setHandleWidth(14);
	Splitter->setStretchFactor(0, 3);
	Splitter->setStretchFactor(1, 4);
	Splitter->setSizes(QList<int>() << 430 << 690);

	QFrame* Frame = new QFrame();
	Frame->setFrameShape(QFrame::HLine);
	Frame->setFrameShadow(QFrame::Raised);
	Layout->addWidget(Frame);

	QHBoxLayout* BottomButtonsLayout = new QHBoxLayout();
	BottomButtonsLayout->addStretch(1);
	QPushButton* SaveInfoButton = new QPushButton("Save Info", &Dialog);
	QPushButton* CancelButton = new QPushButton("Cancel", &Dialog);
	QPushButton* UploadButton = new QPushButton("Upload", &Dialog);
	UploadButton->setDefault(true);
	UploadButton->setAutoDefault(true);
	UploadButton->setStyleSheet("background:#2f8f47; border:1px solid #4fcb72; color:#ffffff; font-weight:700; padding:8px 18px; border-radius:8px;");
	BottomButtonsLayout->addWidget(SaveInfoButton);
	BottomButtonsLayout->addWidget(CancelButton);
	BottomButtonsLayout->addWidget(UploadButton);
	Layout->addLayout(BottomButtonsLayout);

	auto ThumbnailBrowse = [this, ThumbnailEdit]()
	{
		QString FileName = QFileDialog::getOpenFileName(this, "Open Thumbnail", QString(), "All Files (*.*)");
		if (!FileName.isEmpty())
			ThumbnailEdit->setText(FileName);
	};
	auto ShowThumbnailGuide = [this]()
	{
		QDialog GuideDialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
		GuideDialog.setWindowTitle("Thumbnail Guide");
		GuideDialog.resize(640, 420);
		QVBoxLayout* GuideLayout = new QVBoxLayout(&GuideDialog);
		QTextBrowser* GuideText = new QTextBrowser(&GuideDialog);
		GuideText->setOpenExternalLinks(true);
		GuideText->setHtml(
			"<h3>High Quality Steam Workshop Thumbnails</h3>"
			"<p><b>File Size:</b> Under 1MB ( Anything over will result in launcher giving the \"steam error code\" message )</p>"
			"<p><b>File Resolution:</b> 2048x2048 ( You can use any power of 2 as long as the file size is under 1MB )</p>"
			"<p><b>File Format:</b> PNG or JPG ( JPG will have smaller file sizes, but comes with a slight loss in quality )</p>"
			"<h3>High Quality Steam Workshop Preview Images</h3>"
			"<p><b>File Size:</b> Under 2MB</p>"
			"<p><b>File Resolution:</b> 1920 x 1080 ( Any resolution is fine as long as you are under 2MB in file size - some resolutions will be scaled by steam )</p>"
			"<p><b>File Format:</b> PNG or JPG ( JPG will have smaller file sizes, but comes with a slight loss in quality )</p>"
			"<p><i>Gifs are also an option, but make sure to stay under the 1 MB</i></p>");
		GuideLayout->addWidget(GuideText, 1);
		QDialogButtonBox* GuideButtons = new QDialogButtonBox(QDialogButtonBox::Close, &GuideDialog);
		connect(GuideButtons, SIGNAL(rejected()), &GuideDialog, SLOT(reject()));
		GuideLayout->addWidget(GuideButtons);
		GuideDialog.exec();
	};
	auto ShowBriefingDescriptionGuide = [this]()
	{
		QMessageBox::information(this, "Briefing Description Guide", "Briefing Description is uploaded first. Use it for the short in-game description shown to players when the content is installed.");
	};
	auto ShowSteamDescriptionGuide = [this]()
	{
		QMessageBox::information(this, "Steam Description Guide", "Steam Description is pushed automatically right after upload finishes. Leave it blank if you want the Workshop page to reuse the Briefing Description.");
	};
	auto ShowTitleGuide = [this]()
	{
		QMessageBox::information(this, "Title Guide",
			"You can use Treyarch color codes in the title.\n\n"
			"^1 Red\n"
			"^2 Green\n"
			"^3 Yellow\n"
			"^4 Blue\n"
			"^5 Cyan\n"
			"^6 Fuchsia\n"
			"^7 White\n"
			"^8 Light Blue\n"
			"^9 Orange\n"
			"^0 Black\n\n"
			"After upload, the launcher automatically removes these color codes from the Workshop title while keeping the colored title for the briefing upload step.");
	};
	auto RefreshPublishFieldsFromWorkshopJson = [this, WorkshopJsonPath, TitleWidget, BriefingDescriptionWidget, SteamDescriptionWidget, ThumbnailEdit, &TagCheckboxes]()
	{
		QFile WorkshopFile(WorkshopJsonPath);
		if (!WorkshopFile.open(QIODevice::ReadOnly))
			return;
		const QJsonObject Root = QJsonDocument::fromJson(WorkshopFile.readAll()).object();
		mFileId = Root.value("PublisherID").toString().toULongLong();
		TitleWidget->setText(Root.value("Title").toString());
		BriefingDescriptionWidget->setPlainText(Root.value("Description").toString(Root.value("BriefingDescription").toString()));
		SteamDescriptionWidget->setPlainText(Root.value("SteamDescription").toString(BriefingDescriptionWidget->toPlainText()));
		ThumbnailEdit->setText(Root.value("Thumbnail").toString());
		const QStringList VersionTags = Root.value("Tags").toString().split(',', Qt::SkipEmptyParts);
		for (QCheckBox* TagBox : TagCheckboxes)
			TagBox->setChecked(VersionTags.contains(TagBox->text()));
	};

	connect(ThumbnailButton, &QToolButton::clicked, ThumbnailBrowse);
	connect(ThumbnailHelpButton, &QToolButton::clicked, &Dialog, ShowThumbnailGuide);
	connect(TitleHelpButton, &QToolButton::clicked, &Dialog, ShowTitleGuide);
	connect(BriefingDescriptionHelpButton, &QToolButton::clicked, &Dialog, ShowBriefingDescriptionGuide);
	connect(SteamDescriptionHelpButton, &QToolButton::clicked, &Dialog, ShowSteamDescriptionGuide);
	connect(TagsHelpButton, &QToolButton::clicked, &Dialog, [this]()
	{
		QMessageBox::information(this, "Tag Guide", "Choose the tags that best match your map or mod so it is easier to find on the Workshop. You can update tags later too.");
	});
	connect(ThumbnailEdit, &QLineEdit::textChanged, &Dialog, [=](const QString& Text)
	{
		UpdateBackgroundPreviewLabel(ThumbnailPreview, ResolveThumbnailPath(Text));
		RefreshPreviewMeta();
	});
	connect(VersionManagerButton, &QPushButton::clicked, &Dialog, [=, &TagCheckboxes]()
	{
		QTreeWidgetItem TempItem(QStringList() << mFolderName);
		TempItem.setData(0, Qt::UserRole, mType == "map" ? ML_ITEM_MAP : ML_ITEM_MOD_GROUP);
		TempItem.setData(0, ML_ITEM_CONTAINER_ROLE, mFolderName);
		TempItem.setData(0, ML_ITEM_NAME_ROLE, mFolderName);
		ShowWorkshopVersionsDialog(&TempItem);
		RefreshPublishFieldsFromWorkshopJson();
		RefreshPreviewMeta();
	});
	connect(CancelButton, &QPushButton::clicked, &Dialog, &QDialog::reject);
	connect(UploadButton, &QPushButton::clicked, &Dialog, &QDialog::accept);
	connect(SaveInfoButton, &QPushButton::clicked, &Dialog, [=, &Dialog, &TagCheckboxes]()
	{
		mTitle = TitleWidget->text();
		mBriefingDescription = BriefingDescriptionWidget->toPlainText();
		mSteamDescription = SteamDescriptionWidget->toPlainText();
		mThumbnail = ThumbnailEdit->text();
		mTags.clear();
		for (QCheckBox* TagBox : TagCheckboxes)
		{
			if (TagBox->isChecked())
				mTags.append(TagBox->text());
		}

		if (SaveWorkshopMetadata())
			QMessageBox::information(&Dialog, "Workshop Info", QString("Saved %1 without uploading.").arg(QDir::toNativeSeparators(WorkshopJsonPath)));
	});

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mTitle = TitleWidget->text();
	mBriefingDescription = BriefingDescriptionWidget->toPlainText();
	mSteamDescription = SteamDescriptionWidget->toPlainText();
	mThumbnail = ThumbnailEdit->text();
	mTags.clear();
	mPostUploadSteamSyncPending = (StripTreyarchColorCodes(mTitle) != mTitle.trimmed()) || (SteamDescriptionForUpload(mBriefingDescription, mSteamDescription) != mBriefingDescription);

	for (QCheckBox* TagBox : TagCheckboxes)
	{
		if (TagBox->isChecked())
			mTags.append(TagBox->text());
	}

	if (!SteamUGC())
	{
		QMessageBox::information(this, "Error", "Could not initialize Steam, make sure you're running the launcher from the Steam client.");
		return;
	}

	if (!mFileId)
	{
		SteamAPICall_t SteamAPICall = SteamUGC()->CreateItem(AppId, k_EWorkshopFileTypeCommunity);
		mSteamCallResultCreateItem.Set(SteamAPICall, this, &mlMainWindow::OnCreateItemResult);
	}
	else
		UpdateWorkshopItem(true);
}

void mlMainWindow::OnEditOptions()
{
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Options");
	Dialog.resize(980, 760);
	Dialog.setMinimumWidth(920);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QSettings Settings;
	QHBoxLayout* SettingsContentLayout = new QHBoxLayout();
	QListWidget* SettingsNavList = new QListWidget(&Dialog);
	SettingsNavList->setObjectName("SettingsNavList");
	SettingsNavList->setSpacing(2);
	SettingsNavList->setIconSize(QSize(18, 18));
	SettingsNavList->setFixedWidth(188);
	SettingsNavList->setSelectionMode(QAbstractItemView::SingleSelection);
	QStackedWidget* SettingsPages = new QStackedWidget(&Dialog);
	SettingsContentLayout->addWidget(SettingsNavList);
	SettingsContentLayout->addWidget(SettingsPages, 1);
	Layout->addLayout(SettingsContentLayout, 1);
	auto AddSettingsPage = [&](const QString& Label, const QIcon& Icon, QWidget* Page)
	{
		SettingsPages->addWidget(Page);
		QListWidgetItem* Item = new QListWidgetItem(Icon, Label, SettingsNavList);
		Item->setSizeHint(QSize(170, 38));
	};

	QWidget* GeneralTab = new QWidget(&Dialog);
	QVBoxLayout* GeneralLayout = new QVBoxLayout(GeneralTab);
	GeneralLayout->setContentsMargins(14, 14, 14, 14);
	GeneralLayout->setSpacing(12);

	GeneralLayout->addWidget(new QLabel("Pinned Sections"));
	QCheckBox* ShowRecentsCheckbox = new QCheckBox("Enable Recents");
	ShowRecentsCheckbox->setChecked(Settings.value("ShowRecents", true).toBool());
	GeneralLayout->addWidget(ShowRecentsCheckbox);
	QCheckBox* ShowFavoritesCheckbox = new QCheckBox("Enable Favorites");
	ShowFavoritesCheckbox->setChecked(Settings.value("ShowFavorites", true).toBool());
	GeneralLayout->addWidget(ShowFavoritesCheckbox);
	QCheckBox* ShowStartupQuoteCheckbox = new QCheckBox("Show inspirational quote on startup");
	ShowStartupQuoteCheckbox->setChecked(Settings.value("ShowStartupQuote", false).toBool());
	ShowStartupQuoteCheckbox->setToolTip("Show a random quote when the launcher opens.");
	GeneralLayout->addWidget(ShowStartupQuoteCheckbox);

	QFrame* PinnedFrame = new QFrame();
	PinnedFrame->setFrameShape(QFrame::HLine);
	GeneralLayout->addWidget(PinnedFrame);

	QHBoxLayout* LanguageLayout = new QHBoxLayout();
	LanguageLayout->addWidget(new QLabel("Build Language:"));

	QStringList Languages;
	Languages << "All";
	for (int LanguageIdx = 0; LanguageIdx < ARRAYSIZE(gLanguages); LanguageIdx++)
		Languages << gLanguages[LanguageIdx];

	QComboBox* LanguageCombo = new QComboBox();
	LanguageCombo->addItems(Languages);
	LanguageCombo->setCurrentText(mBuildLanguage);
	LanguageLayout->addWidget(LanguageCombo);
	GeneralLayout->addLayout(LanguageLayout);
	QFrame* ApeFrame = new QFrame();
	ApeFrame->setFrameShape(QFrame::HLine);
	GeneralLayout->addWidget(ApeFrame);
	GeneralLayout->addWidget(new QLabel("APE"));
	QLabel* ApeCacheLabel = new QLabel("Delete the Asset Property Editor cache file if APE needs a clean refresh.");
	ApeCacheLabel->setWordWrap(true);
	GeneralLayout->addWidget(ApeCacheLabel);
	QPushButton* DeleteApeCacheButton = new QPushButton("Delete Cache");
	GeneralLayout->addWidget(DeleteApeCacheButton, 0, Qt::AlignLeft);
	GeneralLayout->addStretch(1);
	AddSettingsPage("General", style()->standardIcon(QStyle::SP_FileDialogDetailedView), GeneralTab);

	QList<ToolbarItemConfig> ToolbarItems = LoadToolbarItems();
	int ToolbarSelectedIndex = -1;
	bool ToolbarUiUpdating = false;

	auto ToolbarItemIcon = [&](const ToolbarItemConfig& Item) -> QIcon
	{
		if (!Item.IconPath.trimmed().isEmpty() && QFileInfo(Item.IconPath).exists())
			return QIcon(Item.IconPath);
		if (Item.BuiltIn && Item.BuiltInActionKey == "extra-tools-menu")
			return ToolbarIconForActionKey("extra-tools-menu");
		QAction* BuiltInAction = Item.BuiltIn ? FindBuiltinToolbarAction(this, Item.BuiltInActionKey) : NULL;
		if (BuiltInAction)
			return BuiltInAction->icon();
		return style()->standardIcon(QStyle::SP_FileIcon);
	};

	auto ToolbarItemText = [&](const ToolbarItemConfig& Item) -> QString
	{
		QString Text = Item.Label.isEmpty() ? (Item.BuiltIn ? Item.BuiltInActionKey : QString("Custom Item")) : Item.Label;
		if (Item.Hidden)
			Text += " [hidden]";
		return Text;
	};

	QWidget* ToolbarTab = new QWidget(&Dialog);
	QHBoxLayout* ToolbarLayout = new QHBoxLayout(ToolbarTab);
	ToolbarLayout->setContentsMargins(14, 14, 14, 14);
	ToolbarLayout->setSpacing(12);

	QListWidget* ToolbarList = new QListWidget(ToolbarTab);
	ToolbarList->setObjectName("ToolbarSettingsList");
	ToolbarList->setIconSize(QSize(20, 20));
	ToolbarList->setSelectionMode(QAbstractItemView::SingleSelection);
	ToolbarList->setMinimumWidth(280);
	ToolbarLayout->addWidget(ToolbarList, 0);

	QVBoxLayout* ToolbarDetailLayout = new QVBoxLayout();
	ToolbarDetailLayout->setSpacing(10);
	ToolbarLayout->addLayout(ToolbarDetailLayout, 1);

	QLabel* ToolbarNameLabel = new QLabel("Item:");
	QLabel* ToolbarNameValue = new QLabel("-");
	ToolbarNameValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	ToolbarDetailLayout->addWidget(ToolbarNameLabel);
	ToolbarDetailLayout->addWidget(ToolbarNameValue);

	QLabel* ToolbarFunctionLabel = new QLabel("Functionality:");
	QLabel* ToolbarFunctionValue = new QLabel("-");
	ToolbarFunctionValue->setWordWrap(true);
	ToolbarFunctionValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	ToolbarDetailLayout->addWidget(ToolbarFunctionLabel);
	ToolbarDetailLayout->addWidget(ToolbarFunctionValue);

	QHBoxLayout* ToolbarIconLayout = new QHBoxLayout();
	ToolbarIconLayout->addWidget(new QLabel("Icon:"));
	BackgroundDropLineEdit* ToolbarIconEdit = new BackgroundDropLineEdit();
	ToolbarIconEdit->setPlaceholderText("Drop an icon file here or browse...");
	ToolbarIconLayout->addWidget(ToolbarIconEdit, 1);
	QPushButton* ToolbarIconBrowse = new QPushButton("Browse...");
	ToolbarIconLayout->addWidget(ToolbarIconBrowse);
	ToolbarDetailLayout->addLayout(ToolbarIconLayout);

	QCheckBox* ToolbarHiddenCheck = new QCheckBox("Hide from toolbar");
	ToolbarDetailLayout->addWidget(ToolbarHiddenCheck);

	QFrame* ToolbarDivider = new QFrame();
	ToolbarDivider->setFrameShape(QFrame::HLine);
	ToolbarDetailLayout->addWidget(ToolbarDivider);

	QHBoxLayout* ToolbarButtonsLayout = new QHBoxLayout();
	QPushButton* ToolbarMoveUpButton = new QPushButton("Move Up");
	QPushButton* ToolbarMoveDownButton = new QPushButton("Move Down");
	QPushButton* ToolbarAddButton = new QPushButton("Add Custom Item");
	QPushButton* ToolbarEditButton = new QPushButton("Edit Custom Item");
	QPushButton* ToolbarRemoveButton = new QPushButton("Remove Custom Item");
	QPushButton* ToolbarResetButton = new QPushButton("Reset Defaults");
	ToolbarButtonsLayout->addWidget(ToolbarMoveUpButton);
	ToolbarButtonsLayout->addWidget(ToolbarMoveDownButton);
	ToolbarButtonsLayout->addWidget(ToolbarAddButton);
	ToolbarButtonsLayout->addWidget(ToolbarEditButton);
	ToolbarButtonsLayout->addWidget(ToolbarRemoveButton);
	ToolbarButtonsLayout->addWidget(ToolbarResetButton);
	ToolbarButtonsLayout->addStretch(1);
	ToolbarDetailLayout->addLayout(ToolbarButtonsLayout);
	ToolbarDetailLayout->addStretch(1);

	auto RefreshToolbarRow = [&](int Index)
	{
		if (Index < 0 || Index >= ToolbarItems.count())
			return;
		QListWidgetItem* Row = ToolbarList->item(Index);
		if (!Row)
			return;
		Row->setText(ToolbarItemText(ToolbarItems[Index]));
		Row->setIcon(ToolbarItemIcon(ToolbarItems[Index]));
		Row->setToolTip(ToolbarItems[Index].BuiltIn
			? QString("Built-in toolbar action\n%1").arg(ToolbarItems[Index].BuiltInActionKey)
			: QString("Custom toolbar item\n%1").arg(ToolbarItems[Index].FunctionScript.trimmed()));
	};

	auto RefreshToolbarList = [&]()
	{
		ToolbarUiUpdating = true;
		const int SelectedRow = ToolbarList->currentRow();
		ToolbarList->clear();
		for (int ItemIdx = 0; ItemIdx < ToolbarItems.count(); ItemIdx++)
		{
			QListWidgetItem* Row = new QListWidgetItem(ToolbarItemIcon(ToolbarItems[ItemIdx]), ToolbarItemText(ToolbarItems[ItemIdx]), ToolbarList);
			Row->setData(Qt::UserRole, ItemIdx);
			Row->setToolTip(ToolbarItems[ItemIdx].BuiltIn
				? QString("Built-in toolbar action\n%1").arg(ToolbarItems[ItemIdx].BuiltInActionKey)
				: QString("Custom toolbar item\n%1").arg(ToolbarItems[ItemIdx].FunctionScript.trimmed()));
		}
		if (SelectedRow >= 0 && SelectedRow < ToolbarList->count())
			ToolbarList->setCurrentRow(SelectedRow);
		else if (ToolbarList->count() > 0)
			ToolbarList->setCurrentRow(0);
		ToolbarUiUpdating = false;
	};

	auto LoadToolbarSelection = [&]()
	{
		if (ToolbarUiUpdating)
			return;
		ToolbarSelectedIndex = ToolbarList->currentRow();
		ToolbarUiUpdating = true;
		if (ToolbarSelectedIndex < 0 || ToolbarSelectedIndex >= ToolbarItems.count())
		{
			ToolbarNameValue->setText("-");
			ToolbarFunctionValue->setText("-");
			ToolbarIconEdit->clear();
			ToolbarHiddenCheck->setChecked(false);
			ToolbarEditButton->setEnabled(false);
			ToolbarRemoveButton->setEnabled(false);
			ToolbarMoveUpButton->setEnabled(false);
			ToolbarMoveDownButton->setEnabled(false);
		}
		else
		{
			const ToolbarItemConfig& Item = ToolbarItems[ToolbarSelectedIndex];
			ToolbarNameValue->setText(Item.Label.isEmpty() ? (Item.BuiltIn ? Item.BuiltInActionKey : QString("Custom Item")) : Item.Label);
			ToolbarFunctionValue->setText(Item.BuiltIn
				? QString("Built-in action: %1").arg(Item.BuiltInActionKey)
				: (Item.FunctionScript.trimmed().isEmpty() ? QString("(no functionality yet)") : Item.FunctionScript.trimmed()));
			ToolbarIconEdit->setText(Item.IconPath);
			ToolbarHiddenCheck->setChecked(Item.Hidden);
			ToolbarEditButton->setEnabled(!Item.BuiltIn);
			ToolbarRemoveButton->setEnabled(!Item.BuiltIn);
			ToolbarMoveUpButton->setEnabled(ToolbarSelectedIndex > 0);
			ToolbarMoveDownButton->setEnabled(ToolbarSelectedIndex < ToolbarItems.count() - 1);
		}
		ToolbarIconEdit->setEnabled(ToolbarSelectedIndex >= 0);
		ToolbarIconBrowse->setEnabled(ToolbarSelectedIndex >= 0);
		ToolbarHiddenCheck->setEnabled(ToolbarSelectedIndex >= 0);
		ToolbarUiUpdating = false;
	};

	auto CommitToolbarInlineEdits = [&]()
	{
		if (ToolbarUiUpdating || ToolbarSelectedIndex < 0 || ToolbarSelectedIndex >= ToolbarItems.count())
			return;
		ToolbarItemConfig& Item = ToolbarItems[ToolbarSelectedIndex];
		Item.IconPath = ToolbarIconEdit->text().trimmed();
		Item.Hidden = ToolbarHiddenCheck->isChecked();
		RefreshToolbarRow(ToolbarSelectedIndex);
	};

	auto EditToolbarItemDialog = [&](ToolbarItemConfig* Item, bool IsNew) -> bool
	{
		if (!Item)
			return false;

		QDialog ItemDialog(&Dialog, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
		ItemDialog.setWindowTitle(IsNew ? "Add Toolbar Item" : "Edit Toolbar Item");
		ItemDialog.resize(760, 520);
		QVBoxLayout* ItemLayout = new QVBoxLayout(&ItemDialog);
		QFormLayout* FormLayout = new QFormLayout();
		FormLayout->setLabelAlignment(Qt::AlignLeft);

		QLineEdit* NameEdit = new QLineEdit(Item->Label, &ItemDialog);
		BackgroundDropLineEdit* IconEdit = new BackgroundDropLineEdit(Item->IconPath, &ItemDialog);
		QWidget* IconRow = new QWidget(&ItemDialog);
		QHBoxLayout* IconRowLayout = new QHBoxLayout(IconRow);
		IconRowLayout->setContentsMargins(0, 0, 0, 0);
		IconRowLayout->addWidget(IconEdit, 1);
		QPushButton* IconBrowse = new QPushButton("Browse...", IconRow);
		IconRowLayout->addWidget(IconBrowse);

		QPlainTextEdit* FunctionEdit = new QPlainTextEdit(Item->FunctionScript, &ItemDialog);
		FunctionEdit->setPlaceholderText("One command per line. Supported forms: builtin:<action>, exe:\"path\" [args], url:<url>, file:<path>, folder:<path>.");
		FunctionEdit->setMinimumHeight(240);

		FormLayout->addRow("Name:", NameEdit);
		FormLayout->addRow("Icon:", IconRow);
		FormLayout->addRow("Functionality:", FunctionEdit);
		ItemLayout->addLayout(FormLayout, 1);

		QHBoxLayout* QuickAddLayout = new QHBoxLayout();
		QToolButton* AddBuiltinButton = new QToolButton(&ItemDialog);
		AddBuiltinButton->setText("Add Built-in");
		AddBuiltinButton->setPopupMode(QToolButton::InstantPopup);
		QMenu* BuiltInMenu = new QMenu(AddBuiltinButton);
		for (const ToolbarBuiltinSpec& Spec : kToolbarBuiltinSpecs)
		{
			QAction* AddAction = BuiltInMenu->addAction(QString::fromLatin1(Spec.Label));
			connect(AddAction, &QAction::triggered, &ItemDialog, [FunctionEdit, Spec]()
			{
				if (!FunctionEdit->toPlainText().trimmed().isEmpty())
					FunctionEdit->appendPlainText(QString());
				FunctionEdit->appendPlainText(QString("builtin:%1").arg(QString::fromLatin1(Spec.Key)));
			});
		}
		AddBuiltinButton->setMenu(BuiltInMenu);
		QuickAddLayout->addWidget(AddBuiltinButton);

		QPushButton* AddExecutableButton = new QPushButton("Add Executable", &ItemDialog);
		QuickAddLayout->addWidget(AddExecutableButton);
		QPushButton* AddUrlButton = new QPushButton("Add URL", &ItemDialog);
		QuickAddLayout->addWidget(AddUrlButton);
		QPushButton* AddFolderButton = new QPushButton("Add Folder", &ItemDialog);
		QuickAddLayout->addWidget(AddFolderButton);
		QuickAddLayout->addStretch(1);
		ItemLayout->addLayout(QuickAddLayout);

		auto AppendScriptLine = [&](const QString& Line)
		{
			if (Line.trimmed().isEmpty())
				return;
			if (!FunctionEdit->toPlainText().trimmed().isEmpty())
				FunctionEdit->appendPlainText(QString());
			FunctionEdit->appendPlainText(Line);
		};

		connect(IconBrowse, &QPushButton::clicked, &ItemDialog, [&, IconEdit]()
		{
			const QString FileName = QFileDialog::getOpenFileName(&ItemDialog, "Select Icon", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp *.ico);;All Files (*)");
			if (!FileName.isEmpty())
				IconEdit->setText(FileName);
		});
		connect(AddExecutableButton, &QPushButton::clicked, &ItemDialog, [&, FunctionEdit]()
		{
			const QString FileName = QFileDialog::getOpenFileName(&ItemDialog, "Select Executable", QString(), "Executables (*.exe);;All Files (*)");
			if (!FileName.isEmpty())
				AppendScriptLine(QString("exe:\"%1\"").arg(QDir::toNativeSeparators(FileName)));
		});
		connect(AddUrlButton, &QPushButton::clicked, &ItemDialog, [&, FunctionEdit]()
		{
			bool Accepted = false;
			const QString UrlText = QInputDialog::getText(&ItemDialog, "Add URL", "URL:", QLineEdit::Normal, QString(), &Accepted).trimmed();
			if (Accepted && !UrlText.isEmpty())
				AppendScriptLine(QString("url:%1").arg(UrlText));
		});
		connect(AddFolderButton, &QPushButton::clicked, &ItemDialog, [&, FunctionEdit]()
		{
			const QString FolderPath = QFileDialog::getExistingDirectory(&ItemDialog, "Select Folder", QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
			if (!FolderPath.isEmpty())
				AppendScriptLine(QString("folder:%1").arg(QDir::toNativeSeparators(FolderPath)));
		});

		QDialogButtonBox* Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &ItemDialog);
		ItemLayout->addWidget(Buttons);
		connect(Buttons, SIGNAL(accepted()), &ItemDialog, SLOT(accept()));
		connect(Buttons, SIGNAL(rejected()), &ItemDialog, SLOT(reject()));

		if (ItemDialog.exec() != QDialog::Accepted)
			return false;

		Item->Label = NameEdit->text().trimmed();
		Item->IconPath = IconEdit->text().trimmed();
		Item->FunctionScript = FunctionEdit->toPlainText().trimmed();
		Item->BuiltIn = false;
		Item->BuiltInActionKey.clear();
		if (Item->Key.trimmed().isEmpty())
			Item->Key = QString("custom-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
		return true;
	};

	auto RebuildToolbarPage = [&]()
	{
		RefreshToolbarList();
		LoadToolbarSelection();
	};

	connect(ToolbarList, &QListWidget::currentRowChanged, &Dialog, [=](int)
	{
		LoadToolbarSelection();
	});
	connect(ToolbarIconEdit, &QLineEdit::textChanged, &Dialog, [=](const QString&)
	{
		CommitToolbarInlineEdits();
	});
	connect(ToolbarHiddenCheck, &QCheckBox::toggled, &Dialog, [=](bool)
	{
		CommitToolbarInlineEdits();
	});
	connect(ToolbarIconBrowse, &QPushButton::clicked, &Dialog, [=, &Dialog]()
	{
		const QString FileName = QFileDialog::getOpenFileName(&Dialog, "Select Icon", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp *.ico);;All Files (*)");
		if (!FileName.isEmpty())
			ToolbarIconEdit->setText(FileName);
	});
	connect(ToolbarAddButton, &QPushButton::clicked, &Dialog, [&, ToolbarList]()
	{
		ToolbarItemConfig NewItem;
		NewItem.Key = QString("custom-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
		NewItem.Label = "Custom Item";
		NewItem.BuiltIn = false;
		NewItem.Hidden = false;
		if (!EditToolbarItemDialog(&NewItem, true))
			return;
		ToolbarItems.append(NewItem);
		RefreshToolbarList();
		ToolbarList->setCurrentRow(ToolbarItems.count() - 1);
	});
	connect(ToolbarEditButton, &QPushButton::clicked, &Dialog, [&, ToolbarList]()
	{
		if (ToolbarSelectedIndex < 0 || ToolbarSelectedIndex >= ToolbarItems.count() || ToolbarItems[ToolbarSelectedIndex].BuiltIn)
			return;
		if (!EditToolbarItemDialog(&ToolbarItems[ToolbarSelectedIndex], false))
			return;
		RefreshToolbarList();
		ToolbarList->setCurrentRow(ToolbarSelectedIndex);
	});
	connect(ToolbarRemoveButton, &QPushButton::clicked, &Dialog, [&, ToolbarList]()
	{
		if (ToolbarSelectedIndex < 0 || ToolbarSelectedIndex >= ToolbarItems.count() || ToolbarItems[ToolbarSelectedIndex].BuiltIn)
			return;
		if (QMessageBox::question(&Dialog, "Remove Toolbar Item", QString("Remove '%1'?" ).arg(ToolbarItems[ToolbarSelectedIndex].Label)) != QMessageBox::Yes)
			return;
		ToolbarItems.removeAt(ToolbarSelectedIndex);
		RefreshToolbarList();
		ToolbarList->setCurrentRow(qMin(ToolbarSelectedIndex, ToolbarItems.count() - 1));
	});
	connect(ToolbarResetButton, &QPushButton::clicked, &Dialog, [&]()
	{
		if (QMessageBox::question(&Dialog, "Reset Toolbar", "Restore the default toolbar layout and remove custom items?") != QMessageBox::Yes)
			return;
		ToolbarItems = DefaultToolbarItems();
		RefreshToolbarList();
	});
	connect(ToolbarMoveUpButton, &QPushButton::clicked, &Dialog, [&, ToolbarList]()
	{
		if (ToolbarSelectedIndex <= 0 || ToolbarSelectedIndex >= ToolbarItems.count())
			return;
		qSwap(ToolbarItems[ToolbarSelectedIndex - 1], ToolbarItems[ToolbarSelectedIndex]);
		RefreshToolbarList();
		ToolbarList->setCurrentRow(ToolbarSelectedIndex - 1);
	});
	connect(ToolbarMoveDownButton, &QPushButton::clicked, &Dialog, [&, ToolbarList]()
	{
		if (ToolbarSelectedIndex < 0 || ToolbarSelectedIndex >= ToolbarItems.count() - 1)
			return;
		qSwap(ToolbarItems[ToolbarSelectedIndex], ToolbarItems[ToolbarSelectedIndex + 1]);
		RefreshToolbarList();
		ToolbarList->setCurrentRow(ToolbarSelectedIndex + 1);
	});

	RefreshToolbarList();
	LoadToolbarSelection();
	AddSettingsPage("Toolbar", style()->standardIcon(QStyle::SP_TitleBarMenuButton), ToolbarTab);

	QWidget* ThemesTab = new QWidget(&Dialog);
	QVBoxLayout* ThemesLayout = new QVBoxLayout(ThemesTab);
	ThemesLayout->setContentsMargins(14, 14, 14, 14);
	ThemesLayout->setSpacing(12);

	ThemesLayout->addWidget(new QLabel("Theme"));
	QHBoxLayout* ThemePickerLayout = new QHBoxLayout();
	QComboBox* ThemeProfileCombo = new QComboBox(&Dialog);
	for (const QString& ThemeProfileId : AvailableThemeProfileIds())
		ThemeProfileCombo->addItem(ThemeProfileDisplayName(ThemeProfileId), ThemeProfileId);
	const int CurrentThemeIndex = ThemeProfileCombo->findData(CurrentThemeProfileId());
	ThemeProfileCombo->setCurrentIndex(CurrentThemeIndex >= 0 ? CurrentThemeIndex : 0);
	ThemePickerLayout->addWidget(ThemeProfileCombo, 1);
	QPushButton* SaveThemeButton = new QPushButton("Save Theme");
	SaveThemeButton->setToolTip("Save the current theme settings into the selected theme profile.");
	ThemePickerLayout->addWidget(SaveThemeButton);
	QPushButton* SaveThemeAsButton = new QPushButton("Save As New");
	SaveThemeAsButton->setToolTip("Create a new saved theme from the current settings.");
	ThemePickerLayout->addWidget(SaveThemeAsButton);
	QPushButton* DeleteThemeButton = new QPushButton("Delete");
	DeleteThemeButton->setToolTip("Delete the selected custom theme.");
	ThemePickerLayout->addWidget(DeleteThemeButton);
	ThemesLayout->addLayout(ThemePickerLayout);

	QHBoxLayout* BaseThemeLayout = new QHBoxLayout();
	BaseThemeLayout->addWidget(new QLabel("Base Style:"));
	QComboBox* BaseThemeCombo = new QComboBox(&Dialog);
	BaseThemeCombo->addItem("Original Updated", ThemeOriginalUpdated);
	BaseThemeCombo->addItem("Original Classic", ThemeOriginalClassic);
	BaseThemeCombo->addItem("Dark Modern", ThemeDarkModern);
	BaseThemeCombo->setCurrentIndex(qMax(0, BaseThemeCombo->findData(mThemeMode)));
	BaseThemeLayout->addWidget(BaseThemeCombo, 1);
	ThemesLayout->addLayout(BaseThemeLayout);

	QFrame* ThemeFrame = new QFrame();
	ThemeFrame->setFrameShape(QFrame::HLine);
	ThemesLayout->addWidget(ThemeFrame);

	ThemesLayout->addWidget(new QLabel("Appearance"));

	QHBoxLayout* AccentLayout = new QHBoxLayout();
	AccentLayout->addWidget(new QLabel("Accent Color:"));
	QLineEdit* AccentColorEdit = new QLineEdit(Settings.value("AccentColor", "#ff8a2a").toString());
	AccentLayout->addWidget(AccentColorEdit);
	QPushButton* AccentBrowseButton = new QPushButton();
	AccentBrowseButton->setObjectName("AccentSwatchButton");
	AccentBrowseButton->setFixedSize(26, 26);
	AccentBrowseButton->setToolTip("Pick accent color");
	AccentLayout->addWidget(AccentBrowseButton);
	ThemesLayout->addLayout(AccentLayout);

	QCheckBox* ShowItemTypeTags = new QCheckBox("Show Map / Mod tags");
	ShowItemTypeTags->setChecked(Settings.value("ShowItemTypeTags", true).toBool());
	ShowItemTypeTags->setToolTip("Show the small Map/Mod label next to each main item name.");
	ThemesLayout->addWidget(ShowItemTypeTags);
	QCheckBox* UseLegacyToolbarIcons = new QCheckBox("Use legacy toolbar icons");
	UseLegacyToolbarIcons->setChecked(Settings.value("UseLegacyToolbarIcons", false).toBool());
	UseLegacyToolbarIcons->setToolTip("Switch the toolbar back to the older icon set instead of the new artwork.");
	ThemesLayout->addWidget(UseLegacyToolbarIcons);

	QHBoxLayout* AssetBackgroundLayout = new QHBoxLayout();
	AssetBackgroundLayout->addWidget(new QLabel("List Background:"));
	BackgroundDropLineEdit* AssetBackgroundEdit = new BackgroundDropLineEdit(Settings.value("AssetTreeBackgroundImage", "").toString());
	AssetBackgroundLayout->addWidget(AssetBackgroundEdit);
	QSlider* AssetBackgroundOpacitySlider = new QSlider(Qt::Horizontal);
	AssetBackgroundOpacitySlider->setRange(0, 100);
	AssetBackgroundOpacitySlider->setValue(Settings.value("AssetTreeBackgroundOpacity", 100).toInt());
	AssetBackgroundOpacitySlider->setMinimumWidth(110);
	AssetBackgroundLayout->addWidget(AssetBackgroundOpacitySlider);
	QSpinBox* AssetBackgroundOpacity = new QSpinBox();
	AssetBackgroundOpacity->setRange(0, 100);
	AssetBackgroundOpacity->setSuffix("%");
	AssetBackgroundOpacity->setValue(Settings.value("AssetTreeBackgroundOpacity", 100).toInt());
	AssetBackgroundLayout->addWidget(AssetBackgroundOpacity);
	QLabel* AssetBackgroundPreview = new QLabel("None");
	AssetBackgroundPreview->setObjectName("BackgroundPreview");
	AssetBackgroundPreview->setFixedSize(52, 52);
	AssetBackgroundPreview->setAlignment(Qt::AlignCenter);
	AssetBackgroundPreview->setToolTip("Current list background preview");
	AssetBackgroundLayout->addWidget(AssetBackgroundPreview);
	QPushButton* AssetBackgroundBrowse = new QPushButton("Browse...");
	AssetBackgroundLayout->addWidget(AssetBackgroundBrowse);
	QPushButton* AssetBackgroundRemove = new QPushButton("X");
	AssetBackgroundRemove->setToolTip("Remove list background");
	AssetBackgroundRemove->setFixedWidth(28);
	AssetBackgroundLayout->addWidget(AssetBackgroundRemove);
	ThemesLayout->addLayout(AssetBackgroundLayout);

	QHBoxLayout* LogBackgroundLayout = new QHBoxLayout();
	LogBackgroundLayout->addWidget(new QLabel("Log Background:"));
	BackgroundDropLineEdit* LogBackgroundEdit = new BackgroundDropLineEdit(Settings.value("LogBackgroundImage", "").toString());
	LogBackgroundLayout->addWidget(LogBackgroundEdit);
	QSlider* LogBackgroundOpacitySlider = new QSlider(Qt::Horizontal);
	LogBackgroundOpacitySlider->setRange(0, 100);
	LogBackgroundOpacitySlider->setValue(Settings.value("LogBackgroundOpacity", 100).toInt());
	LogBackgroundOpacitySlider->setMinimumWidth(110);
	LogBackgroundLayout->addWidget(LogBackgroundOpacitySlider);
	QSpinBox* LogBackgroundOpacity = new QSpinBox();
	LogBackgroundOpacity->setRange(0, 100);
	LogBackgroundOpacity->setSuffix("%");
	LogBackgroundOpacity->setValue(Settings.value("LogBackgroundOpacity", 100).toInt());
	LogBackgroundLayout->addWidget(LogBackgroundOpacity);
	QLabel* LogBackgroundPreview = new QLabel("None");
	LogBackgroundPreview->setObjectName("BackgroundPreview");
	LogBackgroundPreview->setFixedSize(52, 52);
	LogBackgroundPreview->setAlignment(Qt::AlignCenter);
	LogBackgroundPreview->setToolTip("Current log background preview");
	LogBackgroundLayout->addWidget(LogBackgroundPreview);
	QPushButton* LogBackgroundBrowse = new QPushButton("Browse...");
	LogBackgroundLayout->addWidget(LogBackgroundBrowse);
	QPushButton* LogBackgroundRemove = new QPushButton("X");
	LogBackgroundRemove->setToolTip("Remove log background");
	LogBackgroundRemove->setFixedWidth(28);
	LogBackgroundLayout->addWidget(LogBackgroundRemove);
	ThemesLayout->addLayout(LogBackgroundLayout);

	QFrame* LayoutFrame = new QFrame();
	LayoutFrame->setFrameShape(QFrame::HLine);
	ThemesLayout->addWidget(LayoutFrame);

	ThemesLayout->addWidget(new QLabel("Layout"));
	auto CreateLayoutPreviewIcon = [&](const QString& LayoutKey) -> QIcon
	{
		QPixmap Pixmap(64, 40);
		Pixmap.fill(Qt::transparent);
		QPainter Painter(&Pixmap);
		Painter.setRenderHint(QPainter::Antialiasing, true);
		Painter.setPen(QPen(QColor(90, 98, 108), 1));
		Painter.setBrush(QColor(24, 28, 33));
		Painter.drawRoundedRect(QRectF(1, 1, 62, 38), 6, 6);

		auto DrawPanel = [&](const QRect& Rect, const QColor& Color)
		{
			Painter.setPen(Qt::NoPen);
			Painter.setBrush(Color);
			Painter.drawRoundedRect(Rect, 3, 3);
		};

		if (LayoutKey == "left-build-console")
		{
			DrawPanel(QRect(4, 4, 20, 32), QColor(42, 54, 68));
			DrawPanel(QRect(27, 4, 12, 32), QColor(80, 62, 44));
			DrawPanel(QRect(42, 4, 18, 32), QColor(22, 24, 27));
		}
		else if (LayoutKey == "left-console-build")
		{
			DrawPanel(QRect(4, 4, 20, 32), QColor(42, 54, 68));
			DrawPanel(QRect(27, 4, 18, 32), QColor(22, 24, 27));
			DrawPanel(QRect(48, 4, 12, 32), QColor(80, 62, 44));
		}
		else if (LayoutKey == "original")
		{
			DrawPanel(QRect(4, 4, 38, 16), QColor(40, 52, 66));
			DrawPanel(QRect(45, 4, 15, 16), QColor(80, 62, 44));
			DrawPanel(QRect(4, 23, 56, 13), QColor(22, 24, 27));
		}
		else
		{
			DrawPanel(QRect(4, 4, 38, 14), QColor(40, 52, 66));
			DrawPanel(QRect(45, 4, 15, 14), QColor(80, 62, 44));
			DrawPanel(QRect(4, 21, 56, 15), QColor(22, 24, 27));
		}

		return QIcon(Pixmap);
	};

	QButtonGroup* LayoutGroup = new QButtonGroup(&Dialog);
	QHBoxLayout* LayoutOptionsLayout = new QHBoxLayout();
	struct LayoutOption
	{
		const char* Key;
		const char* Label;
	};
	const LayoutOption LayoutOptions[] =
	{
		{ "original", "Original" },
		{ "left-build-console", "Maps / Build / Console" },
		{ "left-console-build", "Maps / Console / Build" }
	};
	QHash<QString, QAbstractButton*> LayoutButtons;
	for (int LayoutIdx = 0; LayoutIdx < static_cast<int>(sizeof(LayoutOptions) / sizeof(LayoutOptions[0])); LayoutIdx++)
	{
		const LayoutOption Option = LayoutOptions[LayoutIdx];
		QToolButton* LayoutButton = new QToolButton(&Dialog);
		LayoutButton->setCheckable(true);
		LayoutButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		LayoutButton->setIcon(CreateLayoutPreviewIcon(Option.Key));
		LayoutButton->setIconSize(QSize(64, 40));
		LayoutButton->setText(Option.Label);
		LayoutButton->setAutoRaise(false);
		LayoutButton->setMinimumWidth(120);
		LayoutGroup->addButton(LayoutButton);
		LayoutButtons.insert(Option.Key, LayoutButton);
		LayoutOptionsLayout->addWidget(LayoutButton);
	}
	QString SavedLayoutKey = mLauncherLayout.trimmed().isEmpty() ? QString("original") : mLauncherLayout.trimmed().toLower();
	if (SavedLayoutKey == "current")
		SavedLayoutKey = "original";
	if (LayoutButtons.contains(SavedLayoutKey))
		LayoutButtons.value(SavedLayoutKey)->setChecked(true);
	else if (LayoutButtons.contains("original"))
		LayoutButtons.value("original")->setChecked(true);
	ThemesLayout->addLayout(LayoutOptionsLayout);

	QFrame* CustomCssFrame = new QFrame();
	CustomCssFrame->setFrameShape(QFrame::HLine);
	ThemesLayout->addWidget(CustomCssFrame);

	ThemesLayout->addWidget(new QLabel("Custom CSS"));
	QPlainTextEdit* CustomCssEdit = new QPlainTextEdit(Settings.value("CustomStylesheet", "").toString());
	CustomCssEdit->setPlaceholderText("Add extra Qt stylesheet rules here...");
	CustomCssEdit->setMinimumHeight(120);
	ThemesLayout->addWidget(CustomCssEdit);
	QLabel* CustomCssHint = new QLabel("Tip: visible asset rows use custom widgets, so row styling usually needs #ItemRowWidget, #ItemTitleWidget, or #QuickActionStrip rather than only #AssetTree::item.");
	CustomCssHint->setWordWrap(true);
	ThemesLayout->addWidget(CustomCssHint);
	QPushButton* ApplyCustomCssButton = new QPushButton("Apply CSS");
	ApplyCustomCssButton->setToolTip("Apply the custom stylesheet immediately without closing options.");
	ThemesLayout->addWidget(ApplyCustomCssButton, 0, Qt::AlignLeft);

	ThemesLayout->addStretch(1);
	AddSettingsPage("Themes", style()->standardIcon(QStyle::SP_DesktopIcon), ThemesTab);

	QWidget* ConsoleTab = new QWidget(&Dialog);
	QVBoxLayout* ConsoleLayout = new QVBoxLayout(ConsoleTab);
	ConsoleLayout->setContentsMargins(14, 14, 14, 14);
	ConsoleLayout->setSpacing(12);

	ConsoleLayout->addWidget(new QLabel("Console Style"));
	QRadioButton* OriginalConsoleStyle = new QRadioButton("Original");
	QRadioButton* ImprovedConsoleStyle = new QRadioButton("Improved");
	if (UseImprovedConsoleStyle(Settings))
		ImprovedConsoleStyle->setChecked(true);
	else
		OriginalConsoleStyle->setChecked(true);
	ConsoleLayout->addWidget(OriginalConsoleStyle);
	ConsoleLayout->addWidget(ImprovedConsoleStyle);

	QFrame* LogColorsFrame = new QFrame();
	LogColorsFrame->setFrameShape(QFrame::HLine);
	ConsoleLayout->addWidget(LogColorsFrame);

	ConsoleLayout->addWidget(new QLabel("Log Colors"));
	struct LogColorField
	{
		QString Label;
		QString Key;
		QString DefaultValue;
	};
	const LogColorField LogColorFields[] = {
		{ "Default", "LogColors/Default", "#d7dce2" },
		{ "Command", "LogColors/Command", "#7dcfff" },
		{ "Info", "LogColors/Info", "#eef1f4" },
		{ "Launch", "LogColors/Launch", "#c792ea" },
		{ "Success", "LogColors/Success", "#6ee7a8" },
		{ "Warning", "LogColors/Warning", "#ffcf70" },
		{ "Error", "LogColors/Error", "#ff7a7a" }
	};
	QHash<QString, QLineEdit*> LogColorEdits;
	for (int FieldIdx = 0; FieldIdx < ARRAYSIZE(LogColorFields); FieldIdx++)
	{
		const LogColorField& Field = LogColorFields[FieldIdx];
		QHBoxLayout* ColorLayout = new QHBoxLayout();
		ColorLayout->addWidget(new QLabel(Field.Label + ":"));
		QLineEdit* ColorEdit = new QLineEdit(Settings.value(Field.Key, Field.DefaultValue).toString());
		ColorEdit->setPlaceholderText(Field.DefaultValue);
		ColorLayout->addWidget(ColorEdit, 1);
		QPushButton* BrowseButton = new QPushButton();
		BrowseButton->setFixedSize(26, 26);
		BrowseButton->setToolTip(QString("Pick %1 log color").arg(Field.Label.toLower()));
		ColorLayout->addWidget(BrowseButton);
		ConsoleLayout->addLayout(ColorLayout);
		LogColorEdits.insert(Field.Key, ColorEdit);

		auto RefreshButton = [=]()
		{
			const QString SafeColor = NormalizedStoredColor(ColorEdit->text()).isEmpty() ? Field.DefaultValue : NormalizedStoredColor(ColorEdit->text());
			BrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(SafeColor));
		};
		connect(BrowseButton, &QPushButton::clicked, &Dialog, [&, ColorEdit, Field]()
		{
			QColor Color = QColorDialog::getColor(QColor(ColorEdit->text()), &Dialog, QString("Select %1 Log Color").arg(Field.Label));
			if (Color.isValid())
				ColorEdit->setText(Color.name(QColor::HexRgb));
		});
		connect(ColorEdit, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshButton(); });
		RefreshButton();
	}

	ConsoleLayout->addStretch(1);
	AddSettingsPage("Console", style()->standardIcon(QStyle::SP_MessageBoxInformation), ConsoleTab);

	UpdateStatsTimers();
	FlushCurrentItemTime();
	SetCurrentStatsItem(mFileListWidget ? mFileListWidget->currentItem() : NULL);

	QWidget* StatsTab = new QWidget(&Dialog);
	QVBoxLayout* StatsLayout = new QVBoxLayout(StatsTab);
	StatsLayout->setContentsMargins(14, 14, 14, 14);
	StatsLayout->setSpacing(12);
	QHBoxLayout* StatsHeaderLayout = new QHBoxLayout();
	StatsHeaderLayout->setContentsMargins(0, 0, 0, 0);
	StatsHeaderLayout->setSpacing(12);
	StatsHeaderLayout->addWidget(new QLabel("My Stats"));
	StatsHeaderLayout->addStretch(1);
	QLabel* CurrentOpenTimeLabel = new QLabel(StatsTab);
	CurrentOpenTimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	CurrentOpenTimeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	StatsHeaderLayout->addWidget(CurrentOpenTimeLabel);
	auto RefreshCurrentOpenTimeLabel = [this, CurrentOpenTimeLabel]()
	{
		const qint64 CurrentSeconds = mStatsElapsedTimer.isValid() ? qMax<qint64>(0, mStatsElapsedTimer.elapsed() / 1000) : 0;
		CurrentOpenTimeLabel->setText(QString("Current open time: %1").arg(FormatDuration(CurrentSeconds)));
	};
	RefreshCurrentOpenTimeLabel();
	QTimer* StatsOpenTimer = new QTimer(&Dialog);
	StatsOpenTimer->setInterval(1000);
	connect(StatsOpenTimer, &QTimer::timeout, &Dialog, [=]() { RefreshCurrentOpenTimeLabel(); });
	StatsOpenTimer->start();
	StatsLayout->addLayout(StatsHeaderLayout);
	QLabel* StatsIntro = new QLabel("These stats are tracked locally on this PC while you use the launcher.");
	StatsIntro->setWordWrap(true);
	StatsLayout->addWidget(StatsIntro);

	QGridLayout* StatsGrid = new QGridLayout();
	StatsGrid->setHorizontalSpacing(18);
	StatsGrid->setVerticalSpacing(8);
	auto AddStatRow = [&](int Row, const QString& Label, const QString& Value)
	{
		QLabel* LabelWidget = new QLabel(Label, StatsTab);
		QLabel* ValueWidget = new QLabel(Value, StatsTab);
		if (Row >= 15)
		{
			LabelWidget->setStyleSheet("color:#666666;");
			ValueWidget->setStyleSheet("color:#666666;");
		}
		ValueWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
		StatsGrid->addWidget(LabelWidget, Row, 0);
		StatsGrid->addWidget(ValueWidget, Row, 1);
	};
	auto HasPublishedWorkshopId = [&](bool IsMap, const QString& Name) -> bool
	{
		const QString WorkshopJsonPath = IsMap
			? QDir::cleanPath(QString("%1/usermaps/%2/zone/workshop.json").arg(mGamePath, Name))
			: QDir::cleanPath(QString("%1/mods/%2/zone/workshop.json").arg(mGamePath, Name));
		QFile WorkshopFile(WorkshopJsonPath);
		if (!WorkshopFile.open(QIODevice::ReadOnly))
			return false;

		const QJsonObject Root = QJsonDocument::fromJson(WorkshopFile.readAll()).object();
		const QString PublisherId = Root.value("PublisherID").toString().trimmed();
		return !PublisherId.isEmpty() && PublisherId != "0";
	};
	int CurrentMapCount = 0;
	int PublishedMapCount = 0;
	const QString UserMapsFolder = QDir::cleanPath(QString("%1/usermaps").arg(mGamePath));
	const QStringList UserMapFolders = QDir(UserMapsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	for (const QString& MapName : UserMapFolders)
	{
		const QString ZoneFileName = QDir::cleanPath(QString("%1/%2/zone_source/%2.zone").arg(UserMapsFolder, MapName));
		if (!QFileInfo(ZoneFileName).isFile())
			continue;

		CurrentMapCount++;
		if (HasPublishedWorkshopId(true, MapName))
			PublishedMapCount++;
	}

	int CurrentModCount = 0;
	int PublishedModCount = 0;
	const QString ModsFolder = QDir::cleanPath(QString("%1/mods").arg(mGamePath));
	const QStringList ModFolders = QDir(ModsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	for (const QString& ModName : ModFolders)
	{
		bool HasZone = false;
		const char* ZoneNames[4] = { "core_mod", "mp_mod", "cp_mod", "zm_mod" };
		for (int ZoneIdx = 0; ZoneIdx < 4; ZoneIdx++)
		{
			const QString ZoneFileName = QDir::cleanPath(QString("%1/%2/zone_source/%3.zone").arg(ModsFolder, ModName, ZoneNames[ZoneIdx]));
			if (QFileInfo(ZoneFileName).isFile())
			{
				HasZone = true;
				break;
			}
		}

		if (!HasZone)
			continue;

		CurrentModCount++;
		if (HasPublishedWorkshopId(false, ModName))
			PublishedModCount++;
	}
	AddStatRow(0, "Launcher sessions:", QString::number(StatValue("Stats/SessionCount")));
	AddStatRow(1, "Total time wasted:", FormatDuration(StatValue("Stats/TotalOpenSeconds")));
	AddStatRow(2, "Total active time:", FormatDuration(StatValue("Stats/TotalActiveSeconds")));
	AddStatRow(3, "Total idle time:", FormatDuration(StatValue("Stats/TotalIdleSeconds")));
	AddStatRow(4, "Total compiles:", QString::number(StatValue("Stats/TotalCompiles")));
	AddStatRow(5, "Total compile time:", FormatDuration(StatValue("Stats/TotalCompileSeconds")));
	AddStatRow(6, "Total links:", QString::number(StatValue("Stats/TotalLinks")));
	AddStatRow(7, "Total link time:", FormatDuration(StatValue("Stats/TotalLinkSeconds")));
	AddStatRow(8, "Total lights:", QString::number(StatValue("Stats/TotalLights")));
	AddStatRow(9, "Total light time:", FormatDuration(StatValue("Stats/TotalLightSeconds")));
	AddStatRow(10, "Launcher runs:", QString::number(StatValue("Stats/TotalLauncherRuns")));
	AddStatRow(11, "Total plays:", QString::number(StatValue("Stats/TotalGameLaunches")));
	AddStatRow(12, "Total time in game:", FormatDuration(StatValue("Stats/TotalGameSeconds")));
	AddStatRow(13, "Radiant launches:", QString::number(StatValue("Stats/RadiantLaunches")));
	AddStatRow(14, "Time in Radiant:", FormatDuration(StatValue("Stats/RadiantSeconds")));
	AddStatRow(15, "Asset Editor launches:", QString::number(StatValue("Stats/AssetEditorLaunches")));
	AddStatRow(16, "Time in Asset Editor:", FormatDuration(StatValue("Stats/AssetEditorSeconds")));
	AddStatRow(17, "Current maps:", QString::number(CurrentMapCount));
	AddStatRow(18, "Current mods:", QString::number(CurrentModCount));
	AddStatRow(19, "Workshop publishes:", QString::number(StatValue("Stats/TotalWorkshopPublishes")));
	AddStatRow(20, "Published items:", QString::number(PublishedMapCount + PublishedModCount));
	AddStatRow(21, "Published maps:", QString::number(PublishedMapCount));
	AddStatRow(22, "Published mods:", QString::number(PublishedModCount));
	StatsLayout->addLayout(StatsGrid);

	QFrame* StatsDivider = new QFrame();
	StatsDivider->setFrameShape(QFrame::HLine);
	StatsLayout->addWidget(StatsDivider);
	StatsLayout->addWidget(new QLabel("Per Map / Mod Time"));
	QTreeWidget* StatsItemsTree = new QTreeWidget(StatsTab);
	StatsItemsTree->setColumnCount(8);
	StatsItemsTree->setHeaderLabels(QStringList() << "Item" << "Type" << "Time Wasted" << "In-Game" << "Plays" << "Radiant Time" << "Publishes" << "Published");
	StatsItemsTree->setRootIsDecorated(false);
	StatsItemsTree->setAlternatingRowColors(false);
	StatsItemsTree->setUniformRowHeights(true);
	QSettings StatsSettings;
	QHash<QString, qint64> LauncherSecondsByItem;
	QHash<QString, qint64> GameSecondsByItem;
	QHash<QString, qint64> PlayCountByItem;
	QHash<QString, qint64> RadiantSecondsByItem;
	QHash<QString, qint64> PublishCountByItem;
	StatsSettings.beginGroup("Stats");
	StatsSettings.beginGroup("Items");
	const QStringList ItemGroups = StatsSettings.childGroups();
	for (const QString& ItemKey : ItemGroups)
	{
		StatsSettings.beginGroup(ItemKey);
		const qint64 LauncherSeconds = StatsSettings.value("LauncherSeconds", 0).toLongLong();
		StatsSettings.endGroup();
		LauncherSecondsByItem[NormalizeStatsItemKey(ItemKey)] += LauncherSeconds;
	}
	StatsSettings.endGroup();
	StatsSettings.beginGroup("ItemsGameSeconds");
	const QStringList GameKeys = StatsSettings.childKeys();
	for (const QString& ItemKey : GameKeys)
		GameSecondsByItem[NormalizeStatsItemKey(ItemKey)] += StatsSettings.value(ItemKey, 0).toLongLong();
	StatsSettings.endGroup();
	StatsSettings.beginGroup("ItemsPlayCount");
	const QStringList PlayKeys = StatsSettings.childKeys();
	for (const QString& ItemKey : PlayKeys)
		PlayCountByItem[NormalizeStatsItemKey(ItemKey)] += StatsSettings.value(ItemKey, 0).toLongLong();
	StatsSettings.endGroup();
	StatsSettings.beginGroup("ItemsRadiantSeconds");
	const QStringList RadiantKeys = StatsSettings.childKeys();
	for (const QString& ItemKey : RadiantKeys)
		RadiantSecondsByItem[NormalizeStatsItemKey(ItemKey)] += StatsSettings.value(ItemKey, 0).toLongLong();
	StatsSettings.endGroup();
	StatsSettings.beginGroup("ItemsPublishCount");
	const QStringList PublishKeys = StatsSettings.childKeys();
	for (const QString& ItemKey : PublishKeys)
		PublishCountByItem[NormalizeStatsItemKey(ItemKey)] += StatsSettings.value(ItemKey, 0).toLongLong();
	StatsSettings.endGroup();
	StatsSettings.endGroup();
	QSet<QString> AllItemKeys;
	for (auto It = LauncherSecondsByItem.constBegin(); It != LauncherSecondsByItem.constEnd(); ++It)
		AllItemKeys.insert(It.key());
	for (auto It = GameSecondsByItem.constBegin(); It != GameSecondsByItem.constEnd(); ++It)
		AllItemKeys.insert(It.key());
	for (auto It = PlayCountByItem.constBegin(); It != PlayCountByItem.constEnd(); ++It)
		AllItemKeys.insert(It.key());
	for (auto It = RadiantSecondsByItem.constBegin(); It != RadiantSecondsByItem.constEnd(); ++It)
		AllItemKeys.insert(It.key());
	for (auto It = PublishCountByItem.constBegin(); It != PublishCountByItem.constEnd(); ++It)
		AllItemKeys.insert(It.key());
	QString FavoriteModLabel = "-";
	qint64 FavoriteModScore = -1;
	for (const QString& ItemKey : AllItemKeys)
	{
		const qint64 LauncherSeconds = LauncherSecondsByItem.value(ItemKey);
		const qint64 GameSeconds = GameSecondsByItem.value(ItemKey);
		const qint64 PlayCount = PlayCountByItem.value(ItemKey);
		const qint64 RadiantSeconds = RadiantSecondsByItem.value(ItemKey);
		const qint64 PublishCount = PublishCountByItem.value(ItemKey);
		if (LauncherSeconds <= 0 && GameSeconds <= 0 && PlayCount <= 0 && RadiantSeconds <= 0 && PublishCount <= 0)
			continue;

		const bool IsMap = ItemKey.startsWith("map:");
		const bool IsMod = ItemKey.startsWith("mod:");
		const QString InternalName = IsMap ? ItemKey.mid(4) : (IsMod ? ItemKey.mid(4) : ItemKey);
		const bool IsPublished = IsMap ? HasPublishedWorkshopId(true, InternalName) : (IsMod ? HasPublishedWorkshopId(false, InternalName) : false);
		if (IsMod)
		{
			const qint64 FavoriteScore = qMax(GameSeconds, LauncherSeconds);
			if (FavoriteScore > FavoriteModScore)
			{
				FavoriteModScore = FavoriteScore;
				FavoriteModLabel = DisplayNameForStatsKey(ItemKey);
			}
		}

		QTreeWidgetItem* Item = new QTreeWidgetItem(StatsItemsTree);
		Item->setText(0, DisplayNameForStatsKey(ItemKey));
		Item->setText(1, IsMap ? "Map" : (IsMod ? "Mod" : "Item"));
		Item->setText(2, FormatDuration(LauncherSeconds));
		Item->setText(3, FormatDuration(GameSeconds));
		Item->setText(4, QString::number(PlayCount));
		Item->setText(5, FormatDuration(RadiantSeconds));
		Item->setText(6, QString::number(PublishCount));
		Item->setText(7, IsMap || IsMod ? (IsPublished ? "Yes" : "No") : "-");
		Item->setData(0, Qt::UserRole, LauncherSeconds + GameSeconds + RadiantSeconds);
	}
	StatsItemsTree->sortItems(0, Qt::AscendingOrder);
	StatsLayout->addWidget(StatsItemsTree, 1);
	AddStatRow(23, "Favorite mod:", FavoriteModLabel);
	AddSettingsPage("My Stats", style()->standardIcon(QStyle::SP_FileDialogInfoView), StatsTab);

	connect(SettingsNavList, &QListWidget::currentRowChanged, SettingsPages, &QStackedWidget::setCurrentIndex);
	SettingsNavList->setCurrentRow(0);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(&Dialog);
	ButtonBox->setOrientation(Qt::Horizontal);
	ButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ButtonBox->setCenterButtons(true);

	Layout->addWidget(ButtonBox);

	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	connect(AccentBrowseButton, &QPushButton::clicked, &Dialog, [&, AccentColorEdit]()
	{
		QColor Color = QColorDialog::getColor(QColor(AccentColorEdit->text()), &Dialog, "Select Accent Color");
		if (Color.isValid())
			AccentColorEdit->setText(Color.name(QColor::HexRgb));
	});
	connect(AccentColorEdit, &QLineEdit::textChanged, &Dialog, [=](const QString& Text)
	{
		const QColor PreviewColor(Text.trimmed());
		const QString SafeColor = PreviewColor.isValid() ? PreviewColor.name(QColor::HexRgb) : QString("#444444");
		AccentBrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(SafeColor));
	});
	AccentColorEdit->setToolTip("Controls orange-highlighted UI elements like active tabs, buttons, and hover accents.");
	AccentColorEdit->setPlaceholderText("#ff8a2a");
	AccentColorEdit->setText(AccentColorEdit->text());
	connect(AssetBackgroundBrowse, &QPushButton::clicked, &Dialog, [&, AssetBackgroundEdit]()
	{
		QString FileName = QFileDialog::getOpenFileName(&Dialog, "Select List Background", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
		if (!FileName.isEmpty())
			AssetBackgroundEdit->setText(FileName);
	});
	connect(LogBackgroundBrowse, &QPushButton::clicked, &Dialog, [&, LogBackgroundEdit]()
	{
		QString FileName = QFileDialog::getOpenFileName(&Dialog, "Select Log Background", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
		if (!FileName.isEmpty())
			LogBackgroundEdit->setText(FileName);
	});
	connect(AssetBackgroundRemove, &QPushButton::clicked, &Dialog, [=]()
	{
		AssetBackgroundEdit->clear();
	});
	connect(LogBackgroundRemove, &QPushButton::clicked, &Dialog, [=]()
	{
		LogBackgroundEdit->clear();
	});
	connect(DeleteApeCacheButton, &QPushButton::clicked, &Dialog, [this]()
	{
		const QString ApeCachePath = QDir::cleanPath(QString("%1/AssetWorks/apegdt.cache").arg(mGamePath));
		if (!QFileInfo(ApeCachePath).exists())
		{
			QMessageBox::information(this, "APE Cache", "APE cache file was not found.");
			return;
		}

		if (QFile::remove(ApeCachePath))
			QMessageBox::information(this, "APE Cache", "APE cache deleted successfully.");
		else
			QMessageBox::warning(this, "APE Cache", QString("Unable to delete '%1'.").arg(QDir::toNativeSeparators(ApeCachePath)));
	});
	connect(AssetBackgroundOpacitySlider, &QSlider::valueChanged, AssetBackgroundOpacity, &QSpinBox::setValue);
	connect(AssetBackgroundOpacity, QOverload<int>::of(&QSpinBox::valueChanged), AssetBackgroundOpacitySlider, &QSlider::setValue);
	connect(LogBackgroundOpacitySlider, &QSlider::valueChanged, LogBackgroundOpacity, &QSpinBox::setValue);
	connect(LogBackgroundOpacity, QOverload<int>::of(&QSpinBox::valueChanged), LogBackgroundOpacitySlider, &QSlider::setValue);
	connect(AssetBackgroundEdit, &QLineEdit::textChanged, &Dialog, [=](const QString& Text)
	{
		UpdateBackgroundPreviewLabel(AssetBackgroundPreview, Text);
	});
	connect(LogBackgroundEdit, &QLineEdit::textChanged, &Dialog, [=](const QString& Text)
	{
		UpdateBackgroundPreviewLabel(LogBackgroundPreview, Text);
	});
	{
		const QColor PreviewColor(AccentColorEdit->text().trimmed());
		const QString SafeColor = PreviewColor.isValid() ? PreviewColor.name(QColor::HexRgb) : QString("#444444");
		AccentBrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(SafeColor));
	}
	UpdateBackgroundPreviewLabel(AssetBackgroundPreview, AssetBackgroundEdit->text());
	UpdateBackgroundPreviewLabel(LogBackgroundPreview, LogBackgroundEdit->text());

	auto CollectThemeValues = [&]() -> QVariantMap
	{
		QVariantMap Values;
		Values.insert("ThemeMode", ThemeModeToSettingsValue(BaseThemeCombo->currentData().toInt()));
		Values.insert("AccentColor", AccentColorEdit->text().trimmed());
		Values.insert("ShowItemTypeTags", ShowItemTypeTags->isChecked());
		Values.insert("UseLegacyToolbarIcons", UseLegacyToolbarIcons->isChecked());
		Values.insert("CustomStylesheet", CustomCssEdit->toPlainText());
		Values.insert("ConsoleStyle", ImprovedConsoleStyle->isChecked() ? "improved" : "original");
		Values.insert("AssetTreeBackgroundImage", AssetBackgroundEdit->text().trimmed());
		Values.insert("AssetTreeBackgroundOpacity", AssetBackgroundOpacity->value());
		Values.insert("LogBackgroundImage", LogBackgroundEdit->text().trimmed());
		Values.insert("LogBackgroundOpacity", LogBackgroundOpacity->value());
		QString SelectedLayoutKey = SavedLayoutKey;
		for (int LayoutIdx = 0; LayoutIdx < static_cast<int>(sizeof(LayoutOptions) / sizeof(LayoutOptions[0])); LayoutIdx++)
		{
			const LayoutOption Option = LayoutOptions[LayoutIdx];
			if (LayoutButtons.contains(Option.Key) && LayoutButtons.value(Option.Key)->isChecked())
			{
				SelectedLayoutKey = Option.Key;
				break;
			}
		}
		Values.insert("LauncherLayout", SelectedLayoutKey);
		for (int FieldIdx = 0; FieldIdx < ARRAYSIZE(LogColorFields); FieldIdx++)
		{
			const LogColorField& Field = LogColorFields[FieldIdx];
			const QString NormalizedColor = NormalizedStoredColor(LogColorEdits.value(Field.Key)->text());
			Values.insert(Field.Key, NormalizedColor.isEmpty() ? Field.DefaultValue : NormalizedColor);
		}
		return Values;
	};

	auto LoadThemeValuesIntoControls = [&](const QVariantMap& Values)
	{
		const QString ThemeModeSetting = Values.value("ThemeMode", "original-updated").toString();
		int ThemeModeValueFromTheme = ThemeOriginalUpdated;
		if (ThemeModeSetting == "original-classic")
			ThemeModeValueFromTheme = ThemeOriginalClassic;
		else if (ThemeModeSetting == "dark-modern")
			ThemeModeValueFromTheme = ThemeDarkModern;
		BaseThemeCombo->setCurrentIndex(qMax(0, BaseThemeCombo->findData(ThemeModeValueFromTheme)));
		AccentColorEdit->setText(Values.value("AccentColor", "#ff8a2a").toString());
		ShowItemTypeTags->setChecked(Values.value("ShowItemTypeTags", true).toBool());
		UseLegacyToolbarIcons->setChecked(Values.value("UseLegacyToolbarIcons", false).toBool());
		CustomCssEdit->setPlainText(Values.value("CustomStylesheet", "").toString());
		if (Values.value("ConsoleStyle", "improved").toString() == "original")
			OriginalConsoleStyle->setChecked(true);
		else
			ImprovedConsoleStyle->setChecked(true);
		AssetBackgroundEdit->setText(Values.value("AssetTreeBackgroundImage", "").toString());
		AssetBackgroundOpacity->setValue(Values.value("AssetTreeBackgroundOpacity", 100).toInt());
		LogBackgroundEdit->setText(Values.value("LogBackgroundImage", "").toString());
		LogBackgroundOpacity->setValue(Values.value("LogBackgroundOpacity", 100).toInt());
		const QString ThemeLayoutKey = Values.value("LauncherLayout", "original").toString().trimmed().toLower();
		for (int LayoutIdx = 0; LayoutIdx < static_cast<int>(sizeof(LayoutOptions) / sizeof(LayoutOptions[0])); LayoutIdx++)
		{
			const LayoutOption Option = LayoutOptions[LayoutIdx];
			if (LayoutButtons.contains(Option.Key))
				LayoutButtons.value(Option.Key)->setChecked(Option.Key == ThemeLayoutKey);
		}
		for (int FieldIdx = 0; FieldIdx < ARRAYSIZE(LogColorFields); FieldIdx++)
		{
			const LogColorField& Field = LogColorFields[FieldIdx];
			LogColorEdits.value(Field.Key)->setText(Values.value(Field.Key, Field.DefaultValue).toString());
		}
	};

	auto ApplyThemeValuesPreview = [&](const QString& ThemeProfileId)
	{
		const QVariantMap Values = CollectThemeValues();
		QSettings PreviewSettings;
		for (auto It = Values.constBegin(); It != Values.constEnd(); ++It)
			PreviewSettings.setValue(It.key(), It.value());
		PreviewSettings.setValue(kThemeProfileSettingKey, ThemeProfileId);
		mThemeProfileId = ThemeProfileId;
		mThemeMode = ThemeModeFromSettings(PreviewSettings);
		mLauncherLayout = PreviewSettings.value("LauncherLayout", mLauncherLayout).toString().trimmed().toLower();
		ApplyLauncherLayout();
		UpdateTheme();
		PopulateFileList();
	};

	auto RefreshThemeButtons = [&]()
	{
		const QString SelectedThemeProfileId = ThemeProfileCombo->currentData().toString();
		const bool IsBuiltInTheme = SelectedThemeProfileId == "original-updated" || SelectedThemeProfileId == "original-classic" || SelectedThemeProfileId == "dark-modern";
		DeleteThemeButton->setEnabled(!IsBuiltInTheme);
		SaveThemeButton->setText(IsBuiltInTheme ? "Update Built-in" : "Save Theme");
	};

	connect(ThemeProfileCombo, &QComboBox::currentIndexChanged, &Dialog, [=](int)
	{
		const QString ThemeProfileId = ThemeProfileCombo->currentData().toString();
		LoadThemeValuesIntoControls(ThemeProfileValues(ThemeProfileId));
		ApplyThemeProfile(ThemeProfileId);
		ApplyLauncherLayout();
		UpdateTheme();
		PopulateFileList();
		RefreshThemeButtons();
	});
	connect(BaseThemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &Dialog, [=](int)
	{
		ApplyThemeValuesPreview(ThemeProfileCombo->currentData().toString());
	});
	connect(SaveThemeButton, &QPushButton::clicked, &Dialog, [=]()
	{
		const QString ThemeProfileId = ThemeProfileCombo->currentData().toString();
		SaveThemeProfile(ThemeProfileId, ThemeProfileDisplayName(ThemeProfileId), CollectThemeValues());
		ApplyThemeProfile(ThemeProfileId);
		ApplyLauncherLayout();
		UpdateTheme();
		PopulateFileList();
	});
	connect(SaveThemeAsButton, &QPushButton::clicked, &Dialog, [=, &Dialog]()
	{
		bool Accepted = false;
		const QString ThemeName = QInputDialog::getText(&Dialog, "Save Theme As", "Theme name:", QLineEdit::Normal, "", &Accepted).trimmed();
		if (!Accepted || ThemeName.isEmpty())
			return;
		QString ThemeProfileId = SanitizedThemeProfileId(ThemeName);
		QString UniqueThemeProfileId = ThemeProfileId;
		int Suffix = 2;
		while (AvailableThemeProfileIds().contains(UniqueThemeProfileId))
			UniqueThemeProfileId = QString("%1-%2").arg(ThemeProfileId).arg(Suffix++);
		SaveThemeProfile(UniqueThemeProfileId, ThemeName, CollectThemeValues());
		ThemeProfileCombo->addItem(ThemeName, UniqueThemeProfileId);
		ThemeProfileCombo->setCurrentIndex(ThemeProfileCombo->findData(UniqueThemeProfileId));
		ApplyThemeProfile(UniqueThemeProfileId);
		ApplyLauncherLayout();
		UpdateTheme();
		PopulateFileList();
		RefreshThemeButtons();
	});
	connect(DeleteThemeButton, &QPushButton::clicked, &Dialog, [=, &Dialog]()
	{
		const QString ThemeProfileId = ThemeProfileCombo->currentData().toString();
		if (ThemeProfileId == "original-updated" || ThemeProfileId == "original-classic" || ThemeProfileId == "dark-modern")
			return;
		if (QMessageBox::question(&Dialog, "Delete Theme", QString("Delete theme '%1'?").arg(ThemeProfileDisplayName(ThemeProfileId))) != QMessageBox::Yes)
			return;
		QSettings DeleteSettings;
		DeleteSettings.beginGroup(kThemeProfilesGroup);
		DeleteSettings.remove(ThemeProfileId);
		DeleteSettings.endGroup();
		const int RemoveIndex = ThemeProfileCombo->currentIndex();
		ThemeProfileCombo->removeItem(RemoveIndex);
		const int FallbackIndex = qMax(0, ThemeProfileCombo->findData(QString("original-updated")));
		ThemeProfileCombo->setCurrentIndex(FallbackIndex);
	});
	RefreshThemeButtons();

	QTimer* CssApplyTimer = new QTimer(&Dialog);
	CssApplyTimer->setSingleShot(true);
	auto ApplyCustomCssPreview = [=]()
	{
		ApplyThemeValuesPreview(ThemeProfileCombo->currentData().toString());
	};
	connect(CustomCssEdit, &QPlainTextEdit::textChanged, &Dialog, [=]()
	{
		CssApplyTimer->start(200);
	});
	connect(CssApplyTimer, &QTimer::timeout, &Dialog, ApplyCustomCssPreview);
	connect(ApplyCustomCssButton, &QPushButton::clicked, &Dialog, [=]()
	{
		CssApplyTimer->stop();
		ApplyCustomCssPreview();
	});

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mBuildLanguage = LanguageCombo->currentText();
	const QString SelectedThemeProfileId = ThemeProfileCombo->currentData().toString();
	const QVariantMap SelectedThemeValues = CollectThemeValues();

	Settings.setValue("BuildLanguage", mBuildLanguage);
	Settings.setValue("ShowRecents", ShowRecentsCheckbox->isChecked());
	Settings.setValue("ShowFavorites", ShowFavoritesCheckbox->isChecked());
	Settings.setValue("ShowStartupQuote", ShowStartupQuoteCheckbox->isChecked());
	SaveToolbarItems(ToolbarItems);
	SaveThemeProfile(SelectedThemeProfileId, ThemeProfileDisplayName(SelectedThemeProfileId), SelectedThemeValues);
	ApplyThemeProfile(SelectedThemeProfileId);
	if (mStartupQuoteLabel)
	{
		if (ShowStartupQuoteCheckbox->isChecked())
		{
			mStartupQuoteText = RandomStartupQuote();
			mStartupQuoteLabel->setText(mStartupQuoteText);
			mStartupQuoteLabel->show();
		}
		else
		{
			mStartupQuoteText.clear();
			mStartupQuoteLabel->clear();
			mStartupQuoteLabel->hide();
		}
	}

	CreateToolBar();
	ApplyLauncherLayout();
	UpdateTheme();
	PopulateFileList();
}

void mlMainWindow::UpdateTheme()
{
	AppendStartupTrace("UpdateTheme:start");
	QSettings Settings;
	mThemeProfileId = CurrentThemeProfileId();
	QString AccentColor = Settings.value("AccentColor", "#ff8a2a").toString().trimmed();
	if (!QColor(AccentColor).isValid())
		AccentColor = "#ff8a2a";
	const QString AssetTreeBackgroundImage = Settings.value("AssetTreeBackgroundImage", "").toString().trimmed();
	const int AssetTreeBackgroundOpacity = Settings.value("AssetTreeBackgroundOpacity", 100).toInt();
	const QString LogBackgroundImage = Settings.value("LogBackgroundImage", "").toString().trimmed();
	const int LogBackgroundOpacity = Settings.value("LogBackgroundOpacity", 100).toInt();
	const QString CustomStylesheet = Settings.value("CustomStylesheet", "").toString();
	const QString DefaultLogColor = Settings.value("LogColors/Default", "#d7dce2").toString().trimmed();
	const QString AccentHoverLight = QColor(AccentColor).lighter(115).name(QColor::HexRgb);
	const QString AccentHoverDark = AccentColor;
	const QString AssetTreeCachedBackground = PrepareBackgroundImageCache(LauncherDataRoot(), AssetTreeBackgroundImage, AssetTreeBackgroundOpacity);
	mAssetTreeBackgroundCachePath = AssetTreeCachedBackground;
	const QString LogCachedBackground = PrepareBackgroundImageCache(LauncherDataRoot(), LogBackgroundImage, LogBackgroundOpacity);
	mLogBackgroundCachePath = LogCachedBackground;
	const bool ImprovedConsole = UseImprovedConsoleStyle(Settings);
	const bool UseClassicChrome = ThemeUsesClassicChrome(mThemeMode);
	const bool UseUpdatedChrome = ThemeUsesUpdatedChrome(mThemeMode);
	const bool UseDarkModernChrome = ThemeUsesDarkModernChrome(mThemeMode);
	setProperty("themeProfile", mThemeProfileId);
	if (mActionFileNew)
		mActionFileNew->setIcon(ToolbarIconForActionKey("file-new"));
	if (mActionFileAssetEditor)
		mActionFileAssetEditor->setIcon(ToolbarIconForActionKey("file-asset-editor"));
	if (mActionFileLevelEditor)
		mActionFileLevelEditor->setIcon(ToolbarIconForActionKey("file-level-editor"));
	if (mActionFileExport2Bin)
		mActionFileExport2Bin->setIcon(ToolbarIconForActionKey("file-export2bin"));
	if (mActionFileOpenScriptReference)
		mActionFileOpenScriptReference->setIcon(ToolbarIconForActionKey("help-script-reference"));
	if (mActionEditBuild)
		mActionEditBuild->setIcon(ToolbarIconForActionKey("edit-build"));
	if (mActionEditAnalyze)
		mActionEditAnalyze->setIcon(ToolbarIconForActionKey("edit-analyze"));
	if (mActionEditReadyForPublish)
		mActionEditReadyForPublish->setIcon(ToolbarIconForActionKey("edit-ready-for-publish"));
	if (mActionEditPublish)
		mActionEditPublish->setIcon(ToolbarIconForActionKey("edit-publish"));
	UpdateOutputConsoleMode();
	if (mLogFiltersButton)
		mLogFiltersButton->setVisible(ImprovedConsole);
	if (mLogSelectionButton)
		mLogSelectionButton->setVisible(ImprovedConsole);

	if (UseClassicChrome)
	{
		qApp->setStyle("Windows");
		const QString ClassicConsoleBackground = QApplication::palette().color(QPalette::Base).name(QColor::HexRgb);
		QFile file(QString("%1/radiant/stylesheet.qss").arg(mToolsPath));
		file.open(QFile::ReadOnly);
		QString styleSheet = QLatin1String(file.readAll());
		file.close();
			styleSheet += QString(
				"#AssetListPanel { background: transparent; }"
				"#ItemRowWidget { background: #171b20; }"
				"#ItemRowWidget { background: transparent; }"
				"#AssetTree { margin-top: 0; padding: 0; background: #5a5a5a; }"
				"#AssetTree::viewport { background: #5a5a5a; }"
				"#OutputConsole, #OutputConsoleViewport, #OutputConsolePlain, #OutputConsolePlainViewport { background: %1; border: 0; outline: 0; }"
				"#OutputConsole:focus, #OutputConsolePlain:focus, #OutputConsoleViewport:focus, #OutputConsolePlainViewport:focus { border: 0; outline: 0; }"
				"#AssetTree::item { min-height: 16px; padding: 0; margin: 0; color: transparent; }"
			"#AssetTree::item:hover { color: transparent; }"
			"#AssetTree::item:selected { color: transparent; }"
			"#AssetTree::indicator { width: 0px; height: 0px; margin: 0px; }"
			"#OutputConsole::item { padding: 0; margin: 0; border: 0; background: transparent; }"
			"#OutputConsole::branch { width: 0px; background: transparent; }"
				"#ItemTitleWidget, #QuickActionStrip, #ItemInternalNameLabel, #ItemTypeTag { background: transparent; border: 0; }"
				"#ItemTitleWidget[childRow=\"true\"], #QuickActionStrip[childRow=\"true\"], #ItemTitleWidget[hovered=\"true\"], #QuickActionStrip[hovered=\"true\"] { background: transparent; }"
				"#ItemNameButton { background: transparent; border: 0; padding: 0; text-align: left; color: #e9e9e9; font-weight: 400; }"
				"#ItemInternalNameLabel, #ItemTypeTag { color: #9a9a9a; padding: 0; margin-left: 2px; }"
				"#QuickLaunchCombo { min-height: 34px; max-height: 34px; }"
				"#QuickLaunchButton { min-height: 34px; max-height: 34px; padding: 3px 8px; text-align: left; background: #5f5f5f; border: 1px solid #7a7a7a; border-radius: 6px; color: #d8d8d8; }"
				"#QuickLaunchPopup { background: #3d3d3d; border: 1px solid #7a7a7a; }"
				"#QuickLaunchSearch { min-height: 24px; padding: 4px 8px; background: #5f5f5f; border: 1px solid #7a7a7a; color: #e4e4e4; }"
				"#QuickLaunchTabs::tab { padding: 4px 8px; margin-right: 2px; background: #555555; border: 1px solid #767676; color: #d6d6d6; }"
				"#QuickLaunchTabs::tab:selected { background: #707070; color: #111111; }"
				"#QuickLaunchList { border: 1px solid #7a7a7a; background: #4a4a4a; color: #e4e4e4; padding: 2px; }"
				"#QuickLaunchList::item { padding: 2px 6px; border: 1px solid transparent; margin: 0; }"
				"#QuickLaunchList::item:selected { background: #8a8a8a; color: #111111; border-color: #9a9a9a; }"
				"#ItemSelectCheckBox { spacing: 0; padding: 0; margin: 0; }"
				"QCheckBox::indicator { width: 12px; height: 12px; border-radius: 1px; border: 1px solid #7a7a7a; background: #5f5f5f; }"
				"#ItemSelectCheckBox::indicator { width: 12px; height: 12px; margin: 0; border-radius: 1px; border: 1px solid #808080; background: #666666; }"
					"#QuickActionStrip { min-height: 22px; }"
					"#QuickActionButton { min-height: 18px; padding: 0 4px; }"
					"#QuickMiniActionButton { background: transparent; border: 0; padding: 0; min-height: 22px; color: #b1b1b1; }"
					"#DisplayNameAddButton { min-height: 18px; padding: 0; }"
				"#OutputTabs { background: transparent; }"
				"#OutputTabs::tab { padding: 4px 8px; margin: 4px 2px 0 0; min-width: 44px; font-size: 11px; }"
				"#LogFiltersButton { background: transparent; border: 0; padding: 0 8px; min-height: 18px; color: #d8d8d8; font-size: 11px; }"
				"#BackgroundPreview { border: 1px solid #5a5a5a; background: #2a2a2a; color: #cfcfcf; }"
				"#GameStateLabel { color: #7a7a7a; padding-left: 2px; }"
				"#FooterStatusLabel { color: #d8d8d8; padding: 4px 8px; font-weight: 600; }"
				"#FooterRefreshButton { padding: 2px 4px; }"
				"#FooterDownloadButton { min-height: 18px; padding: 3px 8px; }")
			.arg(ClassicConsoleBackground)
			+ QString("#OutputConsole, #OutputConsolePlain { color: %1; }").arg(DefaultLogColor)
			+ "#ItemNameButton:hover { background: transparent; color: #111111; }"
			+ "#ItemTitleWidget[hovered=\"true\"] #ItemNameButton, #ItemTitleWidget[hovered=\"true\"] #ItemInternalNameLabel, #ItemTitleWidget[hovered=\"true\"] #ItemTypeTag { color: #111111; }"
			+ "#ItemTitleWidget[selected=\"true\"] #ItemNameButton, #ItemTitleWidget[selected=\"true\"] #ItemInternalNameLabel, #ItemTitleWidget[selected=\"true\"] #ItemTypeTag { color: #000000; }"
			+ QString("#LogFiltersButton:hover { background: transparent; border: 0; color: %1; }").arg(AccentHoverLight)
			+ QString("#QuickMiniActionButton:hover { border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ "#WorkshopVersionsList { border: 1px solid #5a5a5a; background: #262626; outline: 0; }"
			+ "#WorkshopVersionsList::item { border: 1px solid transparent; border-radius: 10px; margin: 3px 2px; padding: 2px; background: transparent; outline: 0; }"
			+ "#WorkshopVersionsList::item:hover { background: #3a3a3a; border-color: #707070; }"
			+ "#WorkshopVersionsList::item:selected { background: #4a4a4a; border-color: #8d8d8d; }"
			+ "#WorkshopVersionRow { background: #303030; border: 1px solid #444444; border-radius: 8px; }"
			+ "#WorkshopVersionRowActive { background: #3b3f2d; border: 1px solid #8cb667; border-radius: 8px; }"
			+ "#WorkshopVersionTitleLabel, #WorkshopVersionIdLabel { background: transparent; border: 0; }"
			+ "#WorkshopVersionActiveBadge { background: #4f6540; color: #eef6e8; border: 1px solid #8cb667; border-radius: 8px; padding: 2px 8px; font-weight: 700; }"
			+ CustomStylesheet;
		qApp->setStyleSheet(styleSheet);
	}
	else if (UseUpdatedChrome)
	{
		qApp->setStyle("Windows");
		QFile file(QString("%1/radiant/stylesheet.qss").arg(mToolsPath));
		file.open(QFile::ReadOnly);
		QString styleSheet = QLatin1String(file.readAll());
		file.close();
		styleSheet += QString(
			"QPushButton { padding: 6px 10px; border-radius: 4px; }"
					"#AssetListPanel { background: transparent; }"
					"#ItemRowWidget { background: transparent; }"
					"#SettingsNavList { background: #232323; border: 1px solid #3a3a3a; padding: 4px; }"
			"#SettingsNavList::item { padding: 10px 12px; border-radius: 6px; }"
			"#SettingsNavList::item:selected { background: #3b3b3b; color: #f0f0f0; }"
				"#CategoryTabs { background: transparent; }"
					"#CategoryTabs::tab { background: #3a3a3a; color: palette(button-text); border: 1px solid #2a2a2a; border-bottom: 0; padding: 8px 16px 8px 12px; margin: 6px 3px 0 0; min-width: 84px; border-top-left-radius: 8px; border-top-right-radius: 8px; }"
				"#OutputTabs { background: transparent; }"
				"#OutputTabs::tab { background: #2b2b2b; border: 1px solid #1e1e1e; border-bottom: 0; padding: 4px 8px; margin: 4px 2px 0 0; min-width: 48px; border-top-left-radius: 6px; border-top-right-radius: 6px; font-size: 11px; }"
						"#AssetTree { margin-top: 0; border-top-left-radius: 0; padding: 0; background: #4f4f4f; }"
					"#AssetTree::viewport { background: #4f4f4f; }"
				"#AssetTree::item { min-height: 26px; padding: 2px 0; margin: 4px 0; color: transparent; }"
				"#AssetTree::item:hover { background: transparent; color: transparent; }"
				"#AssetTree::item:selected { background: transparent; color: transparent; }"
				"#AssetTree::indicator { width: 0px; height: 0px; margin: 0px; }"
				"#AssetTree::branch { background: transparent; }"
					"#OutputConsole, #OutputConsoleViewport, #OutputConsolePlain, #OutputConsolePlainViewport { background: #161616; }"
				"#OutputConsole::item { padding: 0; margin: 0; border: 0; background: transparent; }"
				"#OutputConsole::branch { width: 0px; background: transparent; }"
					"#ItemTitleWidget, #QuickActionStrip { background: #3a3a3a; border: 0; border-radius: 4px; }"
					"#ItemTitleWidget { border-right: 1px solid #454545; }"
					"#QuickActionStrip { border-left: 1px solid #454545; }"
					"#ItemTitleWidget[childRow=\"true\"], #QuickActionStrip[childRow=\"true\"] { background: #434343; }"
				"#ItemTitleWidget[hovered=\"true\"], #QuickActionStrip[hovered=\"true\"] { background: rgba(138, 83, 22, 168); }"
				"#ItemTitleWidget[hovered=\"true\"] #ItemNameButton, #ItemTitleWidget[hovered=\"true\"] #ItemInternalNameLabel, #ItemTitleWidget[hovered=\"true\"] #ItemTypeTag, #QuickActionStrip[hovered=\"true\"] #QuickActionButton { color: #ffffff; }"
					"#ItemTypeTag { border-radius: 5px; padding: 1px 6px; margin-left: 2px; font-size: 10px; font-weight: 700; border: 1px solid #4d4d4d; background: #404040; color: #c9c9c9; }"
					"#ItemTypeTag[itemType=\"map\"] { background: #444444; color: #d0d0d0; }"
					"#ItemTypeTag[itemType=\"mod\"] { background: #3a3a3a; color: #c4c4c4; }"
			"#ItemNameButton { background: transparent; border: 0; padding: 0; text-align: left; color: #ffffff; font-weight: 600; }"
				"#ItemInternalNameLabel { color: #bcbcbc; background: #3f3f3f; border: 1px solid #575757; border-radius: 5px; padding: 1px 6px; margin-left: 2px; }"
			"#QuickLaunchCombo { min-height: 34px; max-height: 34px; }"
			"#QuickLaunchButton { min-height: 34px; max-height: 34px; padding: 6px 10px; text-align: left; background: #242424; border: 1px solid #3a3a3a; border-radius: 8px; color: #eef1f4; }"
			"#QuickLaunchPopup { background: #2d2d2d; border: 1px solid #505050; border-radius: 10px; }"
			"#QuickLaunchSearch { min-height: 26px; padding: 4px 8px; background: #3a3a3a; border: 1px solid #5a5a5a; border-radius: 8px; color: #f0f0f0; }"
			"#QuickLaunchTabs::tab { padding: 5px 10px; margin-right: 3px; background: #343434; border: 1px solid #4a4a4a; border-radius: 7px; color: #d0d0d0; }"
			"#QuickLaunchTabs::tab:selected { background: #555555; color: #ffffff; }"
			"#QuickLaunchList { border: 1px solid #4f4f4f; border-radius: 10px; background: #353535; color: #f0f0f0; padding: 3px; }"
			"#QuickLaunchList::item { padding: 3px 8px; border: 1px solid transparent; border-radius: 8px; margin: 1px 0; }"
			"#QuickLaunchList::item:selected { background: #525252; color: #ffffff; border-color: #666666; }"
			"#ItemSelectCheckBox { spacing: 0; padding: 0; margin: 0; }"
			"#ItemSelectCheckBox::indicator { width: 14px; height: 14px; margin: 0; }"
			"#QuickActionStrip { min-height: 34px; }"
			"#QuickActionButton { min-height: 22px; padding: 2px 6px; }"
			"#QuickMiniActionButton { background: transparent; border: 0; padding: 0; min-height: 22px; color: #f0f0f0; }"
			"#DisplayNameAddButton { background: #3f3f3f; border: 1px solid #686868; border-radius: 6px; padding: 0; min-height: 22px; color: #f0f0f0; }"
			"#LogFiltersButton { background: transparent; border: 0; padding: 0 8px; min-height: 18px; color: #d8d8d8; font-size: 11px; }"
					"#BackgroundPreview { border: 1px solid #5a5a5a; background: #2f2f2f; color: #cfcfcf; }"
			"QComboBox, QLineEdit, QCheckBox { padding: 4px; }"
			"#SuccessBanner { background: #4f6540; color: #eef6e8; border: 1px solid #8cb667; border-radius: 6px; padding: 8px; }"
			"#WarningBanner { background: #6b4b24; color: #fff0de; border: 1px solid #d58b34; border-radius: 6px; padding: 8px; }"
					"#InfoBanner { background: #575757; color: #ededed; border: 1px solid #9a9a9a; border-radius: 6px; padding: 8px; }"
				"#GameStateLabel { color: #7a7a7a; padding-left: 2px; }"
						"QStatusBar { background: #2a2a2a; border-top: 1px solid #4e4e4e; }"
						"#FooterStatusLabel { color: #d8d8d8; padding: 4px 8px; font-weight: 600; }"
						"#FooterRefreshButton { padding: 2px 4px; }"
						"#FooterDownloadButton { min-height: 18px; padding: 3px 8px; }")
			+ QString("#OutputConsole, #OutputConsolePlain { color: %1; }").arg(DefaultLogColor)
			+ QString("QPushButton:hover { background: #474747; border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ "QToolBar QToolButton { margin: 0; padding: 6px 8px; border: 1px solid transparent; }"
			+ QString("QToolBar QToolButton:hover { margin: 0; padding: 6px 8px; border: 1px solid %1; }").arg(AccentHoverLight)
			+ "QToolBar QToolButton:pressed { margin: 0; padding: 6px 8px; border: 1px solid transparent; }"
			+ QString("#LogFiltersButton:hover { background: transparent; border: 0; color: %1; }").arg(AccentHoverLight)
				+ QString("#CategoryTabs::tab:selected { color: palette(button-text); background: %1; border-color: %1; }").arg(AccentHoverLight)
				+ QString("#CategoryTabs::tab:selected:hover { color: palette(button-text); background: %1; border-color: %1; }").arg(QColor(AccentHoverLight).darker(108).name(QColor::HexRgb))
				+ QString("#CategoryTabs::tab:hover { color: palette(button-text); border-color: %1; }").arg(AccentHoverLight)
				+ QString("#OutputTabs::tab:selected { color: #111111; background: %1; border-color: %1; }").arg(AccentHoverLight)
				+ QString("#OutputTabs::tab:selected:hover { color: #111111; background: %1; border-color: %1; }").arg(QColor(AccentHoverLight).darker(108).name(QColor::HexRgb))
				+ QString("#OutputTabs::tab:hover { border-color: %1; color: %1; }").arg(AccentHoverLight)
				+ QString("QMenuBar::item:selected { border: 1px solid %1; }").arg(AccentHoverLight)
			+ QString("#ItemNameButton:hover { background: transparent; color: %1; }").arg(AccentHoverLight)
			+ QString("#QuickActionButton:hover { background: #474747; border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ QString("#QuickMiniActionButton:hover { border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ QString("#DisplayNameAddButton:hover { background: #505050; border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ QString("QCheckBox::indicator:checked { background: %1; border-color: %1; image: url(:/resources/checkmark_white.svg); }").arg(AccentHoverLight)
			+ "#WorkshopVersionsList { border: 1px solid #5a5a5a; background: #262626; outline: 0; }"
			+ "#WorkshopVersionsList::item { border: 1px solid transparent; border-radius: 10px; margin: 3px 2px; padding: 2px; background: transparent; outline: 0; }"
			+ "#WorkshopVersionsList::item:hover { background: #3a3a3a; border-color: #707070; }"
			+ "#WorkshopVersionsList::item:selected { background: #4a4a4a; border-color: #8d8d8d; }"
			+ "#WorkshopVersionRow { background: #303030; border: 1px solid #444444; border-radius: 8px; }"
			+ "#WorkshopVersionRowActive { background: #3b3f2d; border: 1px solid #8cb667; border-radius: 8px; }"
			+ "#WorkshopVersionTitleLabel, #WorkshopVersionIdLabel { background: transparent; border: 0; }"
			+ "#WorkshopVersionActiveBadge { background: #4f6540; color: #eef6e8; border: 1px solid #8cb667; border-radius: 8px; padding: 2px 8px; font-weight: 700; }"
			+ CustomStylesheet;
		qApp->setStyleSheet(styleSheet);
	}
	else if (UseDarkModernChrome)
	{
		qApp->setStyle("Fusion");
		qApp->setStyleSheet(
			QString(
					"QMainWindow, QDialog { background: #141414; color: #eef1f4; }"
					"#SettingsNavList { background: #1a1a1a; border: 1px solid #323232; padding: 4px; }"
			"#SettingsNavList::item { padding: 10px 12px; border-radius: 8px; }"
						"#SettingsNavList::item:selected { background: #303030; color: #eef1f4; }"
				"#AssetListPanel { background: transparent; }"
				"#CategoryTabs { background: transparent; }"
					"#CategoryTabs::tab { background: #2d2d2d; color: palette(button-text); border: 1px solid #444444; border-bottom: 0; padding: 9px 17px 9px 13px; margin: 8px 4px 0 0; min-width: 84px; border-top-left-radius: 10px; border-top-right-radius: 10px; }"
				"#OutputTabs { background: transparent; }"
				"#OutputTabs::tab { background: #1d1d1d; color: #aeb7c0; border: 1px solid #363636; border-bottom: 0; padding: 4px 8px; margin: 4px 2px 0 0; min-width: 48px; border-top-left-radius: 7px; border-top-right-radius: 7px; font-size: 11px; }"
						"QMenuBar, QToolBar { background: #2b2b2b; border: 0; spacing: 6px; padding: 8px; }"
				"QMenuBar::item { background: transparent; padding: 6px 10px; border-radius: 8px; }"
						"QMenuBar::item:selected, QMenu::item:selected { background: #454545; }"
						"QMenu { background: #252525; border: 1px solid #454545; padding: 6px; }"
			"QMenu::item { padding: 8px 22px; border-radius: 8px; }"
			"QMenu::indicator { width: 14px; height: 14px; }"
					"QSplitter::handle { background: #262626; }"
					"#ActionsPanel { background: #1b1b1b; border: 1px solid #363636; border-radius: 12px; }"
			"#AssetTree, #OutputConsole, QTextEdit, QTextBrowser, QLineEdit, QComboBox, QTreeWidget, QPlainTextEdit {"
					" background: #1b1b1b; border: 1px solid #363636; border-radius: 12px; color: #eef1f4; padding: 6px; selection-background-color: #303030; selection-color: #ffffff; }"
						"#AssetTree { margin-top: 0; border-top-left-radius: 0; padding: 0; background: #1b1b1b; }"
						"#AssetTree::viewport { background: #1b1b1b; }"
			"#AssetTree { show-decoration-selected: 0; }"
			"#AssetTree::branch, #AssetTree::branch:selected { background: transparent; }"
						"#AssetTree::item { padding: 2px 0; margin: 4px 0; border-radius: 10px; background: transparent; color: transparent; }"
						"#AssetTree::item:hover { background: transparent; color: transparent; }"
						"#AssetTree::item:selected { background: transparent; color: transparent; }"
			"#AssetTree::indicator { width: 0px; height: 0px; margin: 0px; }"
					"#OutputConsole, #OutputConsoleViewport, #OutputConsolePlain, #OutputConsolePlainViewport { background: #181818; }"
			"#OutputConsole::item { padding: 0; margin: 0; border: 0; background: transparent; }"
			"#OutputConsole::branch { width: 0px; background: transparent; }"
						"#ItemRowWidget { background: transparent; }"
						"#ItemTitleWidget, #QuickActionStrip { background: #2d2d2d; border: 0; border-radius: 10px; }"
						"#ItemTitleWidget { border-right: 1px solid #3b3b3b; }"
						"#QuickActionStrip { border-left: 1px solid #3b3b3b; }"
						"#ItemTitleWidget[childRow=\"true\"], #QuickActionStrip[childRow=\"true\"] { background: #383838; }"
						"#ItemTitleWidget[hovered=\"true\"], #QuickActionStrip[hovered=\"true\"] { background: #494949; }"
			"#ItemTitleWidget[hovered=\"true\"] #ItemNameButton, #ItemTitleWidget[hovered=\"true\"] #ItemInternalNameLabel, #ItemTitleWidget[hovered=\"true\"] #ItemTypeTag, #QuickActionStrip[hovered=\"true\"] #QuickActionButton { color: #ffffff; }"
			"#ItemTypeTag { border-radius: 5px; padding: 1px 6px; margin-left: 2px; font-size: 10px; font-weight: 700; border: 1px solid #1f1f1f; background: #252525; color: #8b8b8b; }"
			"#ItemTypeTag[itemType=\"map\"] { background: #22272b; color: #8c98a2; }"
			"#ItemTypeTag[itemType=\"mod\"] { background: #2a2320; color: #a08a7a; }"
			"#ItemNameButton { background: transparent; border: 0; padding: 0; color: #eef1f4; text-align: left; font-weight: 600; }"
			"#ItemInternalNameLabel { color: #cfcfcf; background: #232323; border: 1px solid #2f2f2f; border-radius: 5px; padding: 1px 6px; margin-left: 4px; }"
				"QPushButton { background: #242424; border: 1px solid #3a3a3a; border-radius: 8px; padding: 10px 16px; color: #eef1f4; min-height: 18px; }"
				"#OpenFoldersButton { padding: 5px 8px; min-height: 22px; }"
			"#ActionsPanel QPushButton { padding: 7px 16px; min-height: 14px; }"
			"QPushButton:disabled { color: #7f8993; background: #1b1b1b; border-color: #363636; }"
			"#QuickActionStrip { min-height: 34px; }"
			"#QuickActionButton { background: #222222; border: 1px solid #3a3a3a; border-radius: 5px; padding: 2px 6px; min-height: 22px; }"
			"#QuickActionButton:pressed { background: #343434; }"
			"#QuickMiniActionButton { background: transparent; border: 0; padding: 0; color: #eef1f4; min-height: 22px; }"
			"#DisplayNameAddButton { background: #242424; border: 1px solid #3a3a3a; border-radius: 6px; padding: 0; color: #eef1f4; min-height: 22px; }"
			"#LogFiltersButton { background: transparent; border: 0; padding: 0 8px; color: #cbd2d9; min-height: 18px; font-size: 11px; }"
			"#QuickMiniActionButton:checked { background: #232323; border-color: #5a5a5a; color: #ffffff; }"
			"#BackgroundPreview { border: 1px solid #3a3a3a; background: #1b1b1b; color: #a8a8a8; }"
			"QCheckBox, QLabel { color: #eef1f4; }"
			"QCheckBox::indicator { width: 18px; height: 18px; border-radius: 6px; border: 1px solid #5d5d5d; background: #1d1d1d; }"
			"#ItemSelectCheckBox { spacing: 0; padding: 0; margin: 0; }"
			"#ItemSelectCheckBox::indicator { width: 14px; height: 14px; border-radius: 4px; margin: 0; }"
			"QComboBox { padding: 6px 28px 6px 10px; }"
			"#QuickLaunchCombo { min-height: 36px; max-height: 36px; }"
			"#QuickLaunchButton { min-height: 36px; max-height: 36px; padding: 7px 16px; text-align: left; background: #242424; border: 1px solid #3a3a3a; border-radius: 8px; color: #eef1f4; }"
			"#QuickLaunchPopup { background: #1b1b1b; border: 1px solid #363636; border-radius: 14px; }"
			"#QuickLaunchSearch { min-height: 28px; padding: 5px 9px; background: #202020; border: 1px solid #363636; border-radius: 9px; color: #eef1f4; }"
			"#QuickLaunchTabs::tab { padding: 6px 12px; margin-right: 4px; background: #242424; border: 1px solid #363636; border-radius: 9px; color: #b7bec6; }"
			"#QuickLaunchTabs::tab:selected { background: #353535; color: #ffffff; }"
			"#QuickLaunchList { border: 1px solid #363636; border-radius: 12px; background: #202020; color: #eef1f4; padding: 4px; }"
			"#QuickLaunchList::item { padding: 3px 8px; border: 1px solid transparent; border-radius: 9px; margin: 1px 0; }"
			"#QuickLaunchList::item:selected { background: #303030; color: #ffffff; border-color: #494949; }"
			"QComboBox::drop-down { border: 0; width: 26px; subcontrol-origin: padding; subcontrol-position: top right; }"
			"QComboBox::down-arrow { image: url(:/stylesheet/stylesheet/down_arrow.png); width: 10px; height: 10px; }"
			"QScrollBar:vertical { background: #151515; width: 12px; margin: 8px 0; }"
			"QScrollBar::handle:vertical { background: #424242; min-height: 28px; border-radius: 6px; }"
			"QHeaderView::section { background: #1b1b1b; color: #eef1f4; border: 0; padding: 6px; }"
			"#WarningBanner { background: #2b1f17; border: 1px solid #ff8a2a; border-radius: 12px; padding: 12px; font-weight: 700; }"
			"#SuccessBanner { background: #15241a; border: 1px solid #44d17a; border-radius: 12px; padding: 10px; }"
			"#InfoBanner { background: #232323; border: 1px solid #3a3a3a; border-radius: 12px; padding: 10px; }"
			"#GameStateLabel { color: #8d96a0; padding-left: 2px; }"
				"QStatusBar { background: #1b1b1b; border-top: 1px solid #363636; }"
				"#FooterStatusLabel { color: #cfcfcf; padding: 4px 8px; font-weight: 600; }"
				"#FooterRefreshButton { padding: 2px 4px; }"
				"#FooterDownloadButton { background: #1f2d23; border: 1px solid #35523d; border-radius: 8px; min-height: 18px; padding: 3px 9px; color: #eef6f0; }")
			+ QString("#OutputConsole, #OutputConsolePlain { color: %1; }").arg(DefaultLogColor)
				+ QString("#CategoryTabs::tab:selected { color: palette(button-text); background: %1; border-color: %1; }").arg(AccentHoverDark)
				+ QString("#CategoryTabs::tab:selected:hover { color: palette(button-text); background: %1; border-color: %1; }").arg(QColor(AccentHoverDark).darker(112).name(QColor::HexRgb))
				+ QString("#CategoryTabs::tab:hover { color: palette(button-text); border-color: %1; }").arg(AccentHoverDark)
				+ QString("#OutputTabs::tab:selected { color: #ffffff; background: %1; border-color: %1; }").arg(AccentHoverDark)
				+ QString("#OutputTabs::tab:selected:hover { color: #ffffff; background: %1; border-color: %1; }").arg(QColor(AccentHoverDark).darker(112).name(QColor::HexRgb))
				+ QString("#OutputTabs::tab:hover { color: #eef1f4; border-color: %1; }").arg(AccentHoverDark)
				+ QString("#ItemNameButton:hover { background: transparent; color: %1; }").arg(AccentHoverDark)
			+ QString("QPushButton:hover { background: #303030; border-color: %1; color: %1; }").arg(AccentHoverDark)
			+ QString("#LogFiltersButton:hover { background: transparent; border: 0; color: %1; }").arg(AccentHoverDark)
			+ QString("#BuildButton, #BuildEnglishButton { background: #20262d; border-color: %1; color: #ffffff; font-weight: 700; }").arg(AccentHoverDark)
			+ "#BuildButton:disabled, #BuildEnglishButton:disabled { background: #1b1b1b; border-color: #363636; color: #7f8993; }"
			+ QString("#QuickActionButton:hover { background: #343434; border-color: %1; color: %1; }").arg(AccentHoverDark)
			+ QString("#QuickMiniActionButton:hover { background: #343434; border-color: %1; color: %1; }").arg(AccentHoverDark)
			+ QString("#DisplayNameAddButton:hover { background: #343434; border-color: %1; color: %1; }").arg(AccentHoverDark)
			+ QString("QCheckBox::indicator:checked { background: %1; border-color: %1; image: url(:/resources/checkmark_white.svg); }").arg(AccentHoverDark)
			+ "#WorkshopVersionsList { border: 1px solid #2d353d; border-radius: 12px; background: #171b20; outline: 0; }"
			+ "#WorkshopVersionsList::item { border: 1px solid transparent; border-radius: 12px; margin: 3px 2px; padding: 2px; background: transparent; outline: 0; }"
			+ QString("#WorkshopVersionsList::item:hover { background: #222a31; border-color: %1; }").arg(AccentHoverDark)
			+ QString("#WorkshopVersionsList::item:selected { background: #252e36; border-color: %1; }").arg(AccentHoverDark)
			+ "#WorkshopVersionRow { background: #1d2329; border: 1px solid #2d353d; border-radius: 10px; }"
			+ QString("#WorkshopVersionRowActive { background: #1f2d23; border: 1px solid %1; border-radius: 10px; }").arg(AccentHoverDark)
			+ "#WorkshopVersionTitleLabel, #WorkshopVersionIdLabel { background: transparent; border: 0; }"
			+ QString("#WorkshopVersionActiveBadge { background: %1; color: #ffffff; border: 1px solid %1; border-radius: 8px; padding: 2px 8px; font-weight: 700; }").arg(AccentHoverDark)
			+ CustomStylesheet
		);
	}
	UpdateThemeMenuChecks();
	UpdateBackgroundOverlays();
	if (mQuickLaunchWidget)
	{
		if (ThemeUsesDarkModernChrome(mThemeMode))
			mQuickLaunchWidget->SetControlHeights(36, 400);
		else
			mQuickLaunchWidget->SetControlHeights(34, 380);
	}
	AppendStartupTrace("UpdateTheme:done");
}

void mlMainWindow::UpdateQuickLaunchVisibility()
{
	const QTreeWidgetItem* CurrentItem = mFileListWidget ? mFileListWidget->currentItem() : NULL;
	const int ItemType = CurrentItem ? CurrentItem->data(0, Qt::UserRole).toInt() : ML_ITEM_UNKNOWN;
	const bool ShowQuickLaunch = (ItemType == ML_ITEM_MOD || ItemType == ML_ITEM_MOD_GROUP);
	if (mQuickLaunchLabel)
		mQuickLaunchLabel->setVisible(ShowQuickLaunch);
	if (mQuickLaunchWidget)
		mQuickLaunchWidget->setVisible(ShowQuickLaunch);
	if (mAnalyzeItemButton)
		mAnalyzeItemButton->setVisible(ItemType == ML_ITEM_MAP || ItemType == ML_ITEM_MOD || ItemType == ML_ITEM_MOD_GROUP);
}

void mlMainWindow::UpdateCategorySummary(const QHash<QString, QVariantMap>& Lookup, const QStringList& Recents, const QStringList& Favorites)
{
	if (!mCategorySummaryLabel)
		return;

	int MapCount = 0;
	int ModCount = 0;
	for (auto It = Lookup.constBegin(); It != Lookup.constEnd(); ++It)
	{
		const int ItemType = It.value().value("type").toInt();
		if (ItemType == ML_ITEM_MAP)
			MapCount++;
		else if (ItemType == ML_ITEM_MOD_GROUP)
			ModCount++;
	}

	auto CountValidPinned = [&](const QStringList& Keys) -> int
	{
		int Count = 0;
		QSet<QString> Seen;
		for (const QString& Key : Keys)
		{
			const QString NormalizedKey = Key.trimmed().toLower();
			if (NormalizedKey.isEmpty() || Seen.contains(NormalizedKey) || !Lookup.contains(NormalizedKey))
				continue;
			Seen.insert(NormalizedKey);
			Count++;
		}
		return Count;
	};

	const int AllCount = MapCount + ModCount;
	const int RecentCount = CountValidPinned(Recents);
	const int FavoriteCount = CountValidPinned(Favorites);
	mCategorySummaryLabel->setText(QString("All: %1   Recents: %2   Favorites: %3   Mods: %4   Maps: %5")
		.arg(AllCount)
		.arg(RecentCount)
		.arg(FavoriteCount)
		.arg(ModCount)
		.arg(MapCount));
}

void mlMainWindow::OnEditDvars()
{
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Dvar Options");

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QLabel* Label = new QLabel(&Dialog);
	Label->setText("Saved dvars are applied automatically whenever the launcher starts the game.\nEntries marked with * differ from their default value.");
	Layout->addWidget(Label);

	QTreeWidget* DvarTree = new QTreeWidget(&Dialog);
	DvarTree->setColumnCount(2);
	DvarTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	DvarTree->setHeaderLabels(QStringList() << "Dvar" << "Value");
	DvarTree->setUniformRowHeights(true);
	DvarTree->setRootIsDecorated(false);
	Layout->addWidget(DvarTree);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(&Dialog);
	ButtonBox->setOrientation(Qt::Horizontal);
	ButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ButtonBox->setCenterButtons(true);

	Layout->addWidget(ButtonBox);

	for(int DvarIdx = 0; DvarIdx < ARRAYSIZE(gDvars); DvarIdx++)
		Dvar(gDvars[DvarIdx], DvarTree);

	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return;

	for (int ItemIdx = 0; ItemIdx < DvarTree->topLevelItemCount(); ItemIdx++)
	{
		QTreeWidgetItem* Item = DvarTree->topLevelItem(ItemIdx);
		QWidget* widget = DvarTree->itemWidget(Item, 1);
		QString dvarName = Item->data(0, Qt::UserRole).toString();
		dvar_s dvar = Dvar::findDvar(dvarName, gDvars, ARRAYSIZE(gDvars));
		switch(dvar.type)
		{
		case DVAR_VALUE_BOOL:
			Dvar::setDvarSetting(dvar, (QCheckBox*)widget);
			break;
		case DVAR_VALUE_INT:
			Dvar::setDvarSetting(dvar, (QSpinBox*)widget);
			break;
		case DVAR_VALUE_STRING:
			Dvar::setDvarSetting(dvar, (QLineEdit*)widget);
			break;
		}
	}

	RefreshRunDvars();
}

bool mlMainWindow::SaveWorkshopMetadata()
{
	QJsonObject Root;

	if (mFileId)
		Root["PublisherID"] = QString::number(mFileId);
	Root["Title"] = mTitle;
	Root["Description"] = mBriefingDescription;
	Root["SteamDescription"] = mSteamDescription;
	Root["Thumbnail"] = mThumbnail;
	Root["Type"] = mType;
	Root["FolderName"] = mFolderName;
	Root["Tags"] = mTags.join(',');

	QString WorkshopFile(mWorkshopFolder + "/workshop.json");
	QFile File(WorkshopFile);

	if (!File.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, "Error", QString("Error writing to file '%1'.").arg(WorkshopFile));
		return false;
	}

	File.write(QJsonDocument(Root).toJson());
	File.close();
	return true;
}

void mlMainWindow::UpdateWorkshopItem(bool UploadToWorkshop)
{
	if (UploadToWorkshop)
	{
		const QString ContentRoot = QFileInfo(mWorkshopFolder).dir().absolutePath();
		auto ResolveThumbnailPath = [&](const QString& ThumbnailPath) -> QString
		{
			const QString CleanPath = QDir::fromNativeSeparators(ThumbnailPath.trimmed());
			if (CleanPath.isEmpty())
				return QString();
			QFileInfo ThumbnailInfo(CleanPath);
			if (ThumbnailInfo.isAbsolute())
				return ThumbnailInfo.absoluteFilePath();
			return QDir(ContentRoot).absoluteFilePath(CleanPath);
		};
		auto ThumbnailProblems = [&](const QString& ResolvedPath) -> QStringList
		{
			QStringList Problems;
			if (ResolvedPath.isEmpty())
			{
				Problems << "No workshop thumbnail is selected.";
				return Problems;
			}
			QFileInfo ThumbnailInfo(ResolvedPath);
			if (!ThumbnailInfo.exists() || !ThumbnailInfo.isFile())
			{
				Problems << QString("Thumbnail file was not found: %1").arg(QDir::toNativeSeparators(ResolvedPath));
				return Problems;
			}
			const QString Suffix = ThumbnailInfo.suffix().toLower();
			if (Suffix != "png" && Suffix != "jpg" && Suffix != "jpeg")
				Problems << "Thumbnail must be a PNG or JPG image.";
			if (ThumbnailInfo.size() > 1024 * 1024)
				Problems << QString("Thumbnail is %1. Steam workshop thumbnails should stay under 1 MB.").arg(FormatDataSizeBytes(static_cast<quint64>(ThumbnailInfo.size())));
			QImageReader Reader(ResolvedPath);
			const QSize ImageSize = Reader.size();
			if (!ImageSize.isValid())
				Problems << "The selected image could not be read.";
			else
			{
				if (ImageSize.width() != ImageSize.height())
					Problems << QString("Thumbnail should be square. Current size: %1x%2.").arg(ImageSize.width()).arg(ImageSize.height());
				if (!IsPowerOfTwoDimension(ImageSize.width()) || !IsPowerOfTwoDimension(ImageSize.height()))
					Problems << "Thumbnail dimensions should be powers of two for the most reliable upload behavior.";
			}
			return Problems;
		};
		for (;;)
		{
			const QString ResolvedThumbnailPath = ResolveThumbnailPath(mThumbnail);
			const QStringList Problems = ThumbnailProblems(ResolvedThumbnailPath);
			if (Problems.isEmpty())
				break;

			QMessageBox ThumbnailWarning(this);
			ThumbnailWarning.setWindowTitle("Workshop Thumbnail Check");
			ThumbnailWarning.setIcon(QMessageBox::Warning);
			ThumbnailWarning.setText("The current workshop thumbnail may fail upload.");
			ThumbnailWarning.setInformativeText(Problems.join("\n"));
			QAbstractButton* ContinueButton = ThumbnailWarning.addButton("Continue Anyway", QMessageBox::AcceptRole);
			QAbstractButton* ReplaceButton = ThumbnailWarning.addButton("Choose New Thumbnail...", QMessageBox::ActionRole);
			ThumbnailWarning.addButton(QMessageBox::Cancel);
			ThumbnailWarning.exec();
			if (ThumbnailWarning.clickedButton() == ContinueButton)
				break;
			if (ThumbnailWarning.clickedButton() != ReplaceButton)
				return;

			const QString SelectedThumbnail = QFileDialog::getOpenFileName(this, "Select Workshop Thumbnail", QFileInfo(ResolvedThumbnailPath).absolutePath(), "Images (*.png *.jpg *.jpeg)");
			if (SelectedThumbnail.isEmpty())
				return;
			const QString RelativeThumbnail = QDir(ContentRoot).relativeFilePath(QDir::fromNativeSeparators(SelectedThumbnail));
			mThumbnail = RelativeThumbnail.startsWith("..") ? QDir::fromNativeSeparators(SelectedThumbnail) : RelativeThumbnail;
		}
		if (HasOnlyEnglishBuild(ContentRoot))
		{
			if (QMessageBox::warning(this, "English Only Build Detected", "Only English localized build output was detected for this item. Uploading now may ship English-only Workshop files. Continue anyway?", QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
				return;
		}
	}

	if (!SaveWorkshopMetadata())
		return;

	if (!mFileId)
		return;

	mWorkshopUploadInFlight = UploadToWorkshop;
	UGCUpdateHandle_t UpdateHandle = SteamUGC()->StartItemUpdate(AppId, mFileId);
	const QString SubmittedTitle = UploadToWorkshop ? mTitle : StripTreyarchColorCodes(mTitle);
	const QString SubmittedDescription = UploadToWorkshop ? mBriefingDescription : SteamDescriptionForUpload(mBriefingDescription, mSteamDescription);
	SteamUGC()->SetItemTitle(UpdateHandle, SubmittedTitle.toLatin1().constData());
	SteamUGC()->SetItemDescription(UpdateHandle, SubmittedDescription.toLatin1().constData());
	SteamUGC()->SetItemPreview(UpdateHandle, mThumbnail.toLatin1().constData());

	const char* TagList[ARRAYSIZE(gTags)];
	SteamParamStringArray_t Tags;
	Tags.m_ppStrings = TagList;
	Tags.m_nNumStrings = 0;

	for (const QString& Tag : mTags)
	{
		QByteArray TagStr = Tag.toLatin1();

		for (int TagIdx = 0; TagIdx < ARRAYSIZE(gTags); TagIdx++)
		{
			if (TagStr == gTags[TagIdx])
			{
				TagList[Tags.m_nNumStrings++] = gTags[TagIdx];
				if (Tags.m_nNumStrings == ARRAYSIZE(TagList))
					break;
			}
		}
	}

	SteamUGC()->SetItemTags(UpdateHandle, &Tags);

	if (!UploadToWorkshop)
	{
		SteamAPICall_t SteamAPICall = SteamUGC()->SubmitItemUpdate(UpdateHandle, "");
		mSteamCallResultUpdateItem.Set(SteamAPICall, this, &mlMainWindow::OnUpdateItemResult);
		return;
	}

	SteamUGC()->SetItemContent(UpdateHandle, mWorkshopFolder.toLatin1().constData());

	SteamAPICall_t SteamAPICall = SteamUGC()->SubmitItemUpdate(UpdateHandle, "");
	mSteamCallResultUpdateItem.Set(SteamAPICall, this, &mlMainWindow::OnUpdateItemResult);

	QProgressDialog Dialog(this);
	Dialog.setLabelText(QString("Uploading workshop item '%1'...").arg(QString::number(mFileId)));
	Dialog.setCancelButton(NULL);
	Dialog.setWindowModality(Qt::WindowModal);
	Dialog.resize(720, 160);
	if (ThemeUsesDarkModernChrome(mThemeMode))
	{
		Dialog.setStyleSheet(
			"QProgressDialog { background: #111418; color: #eef1f4; }"
			"QLabel { color: #eef1f4; font-weight: 600; }"
			"QProgressBar { background: #1a2026; border: 1px solid #2d353d; border-radius: 10px; text-align: center; min-height: 22px; color: #eef1f4; }"
			"QProgressBar::chunk { background: #ff8a2a; border-radius: 8px; margin: 2px; }");
	}
	Dialog.show();

	for (;;)
	{
		uint64 Processed, Total;

		const auto Status = SteamUGC()->GetItemUpdateProgress(SteamAPICall, &Processed, &Total);
		// if we get an invalid status exit out, it could mean we're finished or there's an actual problem
		if (Status == k_EItemUpdateStatusInvalid)
		{
			break;
		}

		switch (Status)
		{
		case EItemUpdateStatus::k_EItemUpdateStatusInvalid:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Invalid" )));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusPreparingConfig:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Preparing Config")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusPreparingContent:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Preparing Content")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusUploadingContent:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Uploading Content")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusUploadingPreviewFile:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Uploading Preview file")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusCommittingChanges:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Committing Changes")));
			break;
		}

		Dialog.setMaximum(Total);
		Dialog.setValue(Processed);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		Sleep(100);
	}
}

void mlMainWindow::OnCreateItemResult(CreateItemResult_t* CreateItemResult, bool IOFailure)
{
	if (IOFailure)
	{
		QMessageBox::warning(this, "Error", "Disk Read error.");
		return;
	}

	if (CreateItemResult->m_eResult != k_EResultOK)
	{
		QMessageBox::warning(this, "Error", QString("Error creating Steam Workshop item. Error code: %1\nVisit https://steamerrors.com/ for more information.").arg(CreateItemResult->m_eResult));
		return;
	}

	mFileId = CreateItemResult->m_nPublishedFileId;

	UpdateWorkshopItem(true);
}

void mlMainWindow::OnUpdateItemResult(SubmitItemUpdateResult_t* UpdateItemResult, bool IOFailure)
{
	const bool CompletedWorkshopUpload = mWorkshopUploadInFlight;
	mWorkshopUploadInFlight = false;

	if (IOFailure)
	{
		QMessageBox::warning(this, "Error", "Disk Read error.");
		return;
	}

	if (UpdateItemResult->m_eResult != k_EResultOK)
	{
		QMessageBox::warning(this, "Error", QString("Error updating Steam Workshop item. Error code: %1\nVisit https://steamerrors.com/ for more information.").arg(UpdateItemResult->m_eResult));
		return;
	}

	if (CompletedWorkshopUpload)
	{
		IncrementStat("Stats/TotalWorkshopPublishes");
		QString PublishedItemKey;
		if (mType.compare("map", Qt::CaseInsensitive) == 0)
			PublishedItemKey = NormalizeStatsItemKey(QString("map:%1").arg(mFolderName));
		else if (mType.compare("mod", Qt::CaseInsensitive) == 0)
			PublishedItemKey = NormalizeStatsItemKey(QString("mod:%1").arg(mFolderName));
		if (!PublishedItemKey.isEmpty())
			IncrementStat(QString("Stats/ItemsPublishCount/%1").arg(PublishedItemKey));
	}

	if (mPostUploadSteamSyncPending)
	{
		mPostUploadSteamSyncPending = false;
		UpdateWorkshopItem(false);
		return;
	}

	if (QMessageBox::question(this, "Update", "Workshop item successfully updated. Do you want to visit the Workshop page for this item now?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
		ShellExecute(NULL, "open", QString("steam://url/CommunityFilePage/%1").arg(QString::number(mFileId)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
}

void mlMainWindow::OnHelpAbout()
{
	QMessageBox::about(this, "About BO3 Mod Tools Black",
		"BO3 Mod Tools Black\n\n"
		"Original by Treyarch\n"
		"Edits by Sphynx\n\n"
		"Launcher features include:\n"
		"- Tabbed map/mod browsing\n"
		"- Favorites and recents\n"
		"- Display names and notes\n"
		"- Workshop version switching\n"
		"- Background previews and theme options\n"
		"- Publish Check workflow helpers\n\n"
		"See Help > Credits for testing and feedback credits.");
}

void mlMainWindow::OnHelpCredits()
{
	QMessageBox::information(this, "Credits",
		"Original Launcher by Treyarch\n"
		"\n"
		"Edits and new features added by Sphynx\n"
		"\n"
		"Testing help:\n"
		"- GentlemanCheeseMan\n"
		"- APX33\n"
		"- Coolyer\n\n"
		"Feedback, ideas, and change suggestions:\n"
		"- GentlemanCheeseMan\n"
		"- Coolyer");
}

void mlMainWindow::OnHelpGuide()
{
	struct GuidePage
	{
		QString Title;
		QString Body;
	};
	const QList<GuidePage> Pages = {
		{
			"Overview",
			"<h2>BO3 Mod Tools Black &mdash; What's New</h2>"
			"<p>This launcher extends the original Treyarch mod tools launcher with quality-of-life features for map and mod creators.</p>"
			"<p>Use the list on the left to jump to any feature. All settings persist between sessions unless noted otherwise.</p>"
		},
		{
			"Themes",
			"<h2>Themes</h2>"
			"<p>Three visual themes are available under the <b>Themes</b> menu in the menu bar:</p>"
			"<ul>"
			"<li><b>Modern</b> &mdash; Dark gray UI with orange accents (default). Designed for extended sessions.</li>"
			"<li><b>Dark Modern</b> &mdash; Deep charcoal palette with a refined rounded style.</li>"
			"<li><b>Classic</b> &mdash; Close to the original Treyarch launcher look.</li>"
			"</ul>"
			"<p>The active theme is saved and restored on next launch.</p>"
			"<p>Additional cosmetic toggles live in <b>Options &rarr; Settings &rarr; Themes</b>:</p>"
			"<ul>"
			"<li><b>Show item type tags</b> &mdash; Displays Map / Mod labels next to each entry.</li>"
			"<li><b>Use legacy toolbar icons</b> &mdash; Reverts toolbar icons to the classic set.</li>"
			"<li><b>Background preview</b> &mdash; Shows the selected map/mod thumbnail behind the details panel.</li>"
			"</ul>"
		},
		{
			"Quick Launch Map",
			"<h2>Quick Launch Map</h2>"
			"<p>When a mod is selected in the list, a <b>Quick Launch Map</b> button appears in the details panel.</p>"
			"<p>Click it to open a searchable popup with tabbed categories:</p>"
			"<ul>"
			"<li><b>General</b> &mdash; No Map / launch to main menu.</li>"
			"<li><b>Zombies</b> &mdash; All stock zombie maps with their internal codes.</li>"
			"<li><b>MP</b> &mdash; All stock multiplayer maps.</li>"
			"<li><b>Campaign</b> &mdash; Story missions.</li>"
			"</ul>"
			"<p>Type in the search box to filter by name, map code, or detail text. The selected map is stored per session and shown on the button label.</p>"
			"<p>Click <b>No Map</b> at the top of the popup to clear the selection.</p>"
		},
		{
			"Favorites & Recents",
			"<h2>Favorites &amp; Recents</h2>"
			"<p>The <b>Favorites</b> and <b>Recents</b> tabs in the main list let you quickly reach frequently used entries.</p>"
			"<ul>"
			"<li>Right-click an entry and choose <b>Add to Favorites</b> to pin it.</li>"
			"<li>Any build or launch action automatically records the entry to Recents.</li>"
			"<li>Pinned full-mod entries expand their child zone files in both tabs for direct access.</li>"
			"<li>Full-mod entries in Recents are normalized &mdash; multiple actions on the same mod count as one entry.</li>"
			"</ul>"
		},
		{
			"Tab Customization",
			"<h2>Tab Customization</h2>"
			"<p>The category bar supports custom tabs for organizing maps and mods.</p>"
			"<ul>"
			"<li>Click the <b>+</b> tab to create a new custom tab.</li>"
			"<li>Double-click any existing tab to open the tab editor.</li>"
			"<li>Right-click a tab for quick actions: <b>Edit</b> and <b>Delete</b>.</li>"
			"<li>Delete is available for custom tabs only and always asks for confirmation.</li>"
			"<li>In the editor you can set tab name, icon image, tab name color, and included map/mod entries.</li>"
			"</ul>"
			"<p>Tab changes are saved automatically and restored on next launch.</p>"
		},
		{
			"Display Names & Notes",
			"<h2>Display Names &amp; Notes</h2>"
			"<p>Each map and mod can have a custom <b>display name</b> that appears in the list instead of the internal folder name.</p>"
			"<p>A <b>selection color</b> can also be assigned to quickly identify entries visually.</p>"
			"<p>Right-click an entry and choose <b>Information</b> to open the info dialog, where you can:</p>"
			"<ul>"
			"<li>Set or edit the display name and color.</li>"
			"<li>Add free-text notes (stored locally per entry).</li>"
			"<li>View compile and link statistics from the last build.</li>"
			"<li>See estimated playtime accumulated via in-launcher game launches.</li>"
			"</ul>"
		},
		{
			"Workshop Upload",
			"<h2>Workshop Upload</h2>"
			"<p>The <b>Publish to Workshop</b> workflow validates your package before upload:</p>"
			"<ul>"
			"<li>Thumbnail must exist and be a supported image type (JPG or PNG).</li>"
			"<li>Dimensions must be at least 512&times;512 and no larger than 3840&times;2160.</li>"
			"<li>File size must be under 1 MB.</li>"
			"</ul>"
			"<p>If issues are found you are prompted to <b>Continue anyway</b> or <b>Replace thumbnail</b>.</p>"
			"<p>The <b>Ready for Publish</b> action in the toolbar runs an all-languages publish-prep build and can optionally bake map lights.</p>"
		},
		{
			"Build Actions",
			"<h2>Build Actions</h2>"
			"<p>The toolbar and Build menu expose several compile modes:</p>"
			"<ul>"
			"<li><b>Build</b> &mdash; Compile and link the selected zone using the default language.</li>"
			"<li><b>Build All Languages</b> &mdash; Runs the linker for all supported language variants.</li>"
			"<li><b>Build Without Language</b> &mdash; Skips the language pass entirely.</li>"
			"<li><b>Ready for Publish</b> &mdash; Full publish-prep pass with optional light baking.</li>"
			"</ul>"
			"<p>While a build is running, the <b>Cancel</b> button terminates the linker process. Build output is shown live in the console panel.</p>"
		},
		{
			"Game Launch",
			"<h2>Game Launch</h2>"
			"<p>The <b>Start Game</b> button launches BlackOps3.exe as a detached process. This means:</p>"
			"<ul>"
			"<li>The <b>Build</b> button becomes available again while the game is running.</li>"
			"<li>The launcher stays open and functional while you play-test.</li>"
			"</ul>"
			"<p>Online mode launches via Steam with <code>+set fs_game &lt;mod&gt;</code>. Workshop selections add <code>+set workshopid &lt;id&gt;</code> automatically.</p>"
			"<p>Custom dvars can be added in <b>Options &rarr; Dvars</b> and are appended to every launch command.</p>"
		},
		{
			"Console & Output",
			"<h2>Console &amp; Output</h2>"
			"<p>The output console at the bottom of the window shows linker and game log output.</p>"
			"<ul>"
			"<li>Output is color-coded by message type (errors in red, warnings in yellow, info in white).</li>"
			"<li>Color coding can be toggled in <b>Options &rarr; Settings &rarr; Console Display</b>.</li>"
			"<li>Multiple output tabs (Linker, Game Log, etc.) can be reordered by dragging.</li>"
			"<li>The <b>Log Filters</b> button next to the tab bar hides or shows specific message categories.</li>"
			"</ul>"
		},
		{
			"Toolbar Customization",
			"<h2>Toolbar Customization</h2>"
			"<p>Right-click the toolbar to open the <b>Customize Toolbar</b> dialog.</p>"
			"<ul>"
			"<li>Built-in actions can be shown or hidden.</li>"
			"<li>Custom items can be added to chain scripts, executables, URLs, files, or folders.</li>"
			"<li>Custom item types: <code>builtin:</code>, <code>exe:</code>, <code>url:</code>, <code>file:</code>, <code>folder:</code>.</li>"
			"<li>All customizations are saved in settings and restored on next launch.</li>"
			"</ul>"
			"<p>The <b>Extra Tools</b> dropdown in the toolbar provides quick access to Radiant, the Asset Exporter, and other tools.</p>"
		},
		{
			"Auto Updates",
			"<h2>Auto Updates</h2>"
			"<p>The launcher checks for updates automatically on startup (can be disabled in <b>Options &rarr; Settings &rarr; General</b>).</p>"
			"<p>When an update is available, a banner appears in the status bar. Click it or use <b>Help &rarr; Check for Updates</b> to review and install.</p>"
			"<p>The updater is a separate <code>LauncherUpdater.exe</code> process that:</p>"
			"<ul>"
			"<li>Waits for the launcher to exit before applying files.</li>"
			"<li>Creates a backup of replaced files.</li>"
			"<li>Rolls back automatically if anything fails.</li>"
			"<li>Restarts the launcher when done.</li>"
			"</ul>"
		},
		{
			"Startup Quotes",
			"<h2>Startup Quotes</h2>"
			"<p>An optional inspirational quote can be shown each time the launcher starts.</p>"
			"<p>Enable this in <b>Options &rarr; Settings &rarr; General &rarr; Show startup quote</b>.</p>"
			"<p>This option is off by default.</p>"
		},
	};

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Guide \342\200\224 BO3 Mod Tools Black");
	Dialog.resize(860, 560);
	Dialog.setMinimumSize(700, 420);

	QHBoxLayout* RootLayout = new QHBoxLayout(&Dialog);
	RootLayout->setContentsMargins(0, 0, 0, 0);
	RootLayout->setSpacing(0);

	QListWidget* NavList = new QListWidget(&Dialog);
	NavList->setObjectName("SettingsNavList");
	NavList->setFixedWidth(180);
	NavList->setFocusPolicy(Qt::NoFocus);
	NavList->setSpacing(2);
	for (const GuidePage& Page : Pages)
	{
		QListWidgetItem* Item = new QListWidgetItem(Page.Title, NavList);
		Item->setSizeHint(QSize(0, 34));
	}

	QTextBrowser* ContentView = new QTextBrowser(&Dialog);
	ContentView->setOpenExternalLinks(true);
	const QColor GuideBg = Dialog.palette().color(QPalette::Base);
	const QColor GuideText = Dialog.palette().color(QPalette::Text);
	const QColor GuideLink = ThemeUsesClassicChrome(mThemeMode)
		? QColor("#2a74cc")
		: (ThemeUsesDarkModernChrome(mThemeMode) ? QColor("#8dbdff") : QColor("#7ea9ff"));
	ContentView->setStyleSheet(QString("QTextBrowser { border: 0; padding: 0; background: %1; color: %2; } a { color: %3; }")
		.arg(GuideBg.name(QColor::HexRgb), GuideText.name(QColor::HexRgb), GuideLink.name(QColor::HexRgb)));

	RootLayout->addWidget(NavList);
	RootLayout->addWidget(ContentView, 1);

	QObject::connect(NavList, &QListWidget::currentRowChanged, &Dialog, [&](int Row)
	{
		if (Row < 0 || Row >= Pages.count())
			return;
		ContentView->setHtml(
			QString("<html><body style=\"font-family:sans-serif; font-size:13px; color:%1; background:%2; padding:18px; line-height:1.45;\">")
				.arg(GuideText.name(QColor::HexRgb), GuideBg.name(QColor::HexRgb))
			+ Pages[Row].Body
			+ "</body></html>");
	});

	NavList->setCurrentRow(0);

	QPushButton* CloseBtn = new QPushButton("Close", &Dialog);
	connect(CloseBtn, &QPushButton::clicked, &Dialog, &QDialog::accept);

	QVBoxLayout* RightLayout = new QVBoxLayout();
	RightLayout->setContentsMargins(0, 0, 0, 0);
	RightLayout->setSpacing(0);
	RightLayout->addWidget(ContentView, 1);
	QHBoxLayout* FooterRow = new QHBoxLayout();
	FooterRow->setContentsMargins(12, 8, 12, 10);
	FooterRow->addStretch();
	FooterRow->addWidget(CloseBtn);
	RightLayout->addLayout(FooterRow);

	// Replace placeholder content layout with proper right panel
	RootLayout->removeWidget(ContentView);
	RootLayout->addLayout(RightLayout);

	Dialog.exec();
}

void mlMainWindow::OnCheckForUpdates()
{
	CheckForUpdates(true);
}

void mlMainWindow::OnOpenZoneFile()
{
	QTreeWidgetItem* Item = ActiveFileItem();
	if (!Item)
		return;

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = GetItemEntryName(Item);
		ShellExecute(NULL, "open", QString("\"%1/usermaps/%2/zone_source/%3.zone\"").arg(mGamePath, MapName, MapName).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
	else
	{
		QString ModName = GetItemContainerName(Item);
		QString ZoneName = GetItemEntryName(Item);
		if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MOD_GROUP)
			return;
		ShellExecute(NULL, "open", (QString("\"%1/mods/%2/zone_source/%3.zone\"").arg(mGamePath, ModName, ZoneName)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
}

void mlMainWindow::OnOpenModRootFolder()
{
	if (mGamePath.trimmed().isEmpty())
		return;

	QTreeWidgetItem* Item = ActiveFileItem();
	QString Folder = QDir::cleanPath(mGamePath);
	if (Item)
	{
		if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
			Folder = QDir::cleanPath(QString("%1/usermaps/%2").arg(mGamePath, GetItemEntryName(Item)));
		else
			Folder = QDir::cleanPath(QString("%1/mods/%2").arg(mGamePath, GetItemContainerName(Item)));
	}

	ShellExecute(NULL, "open", (QString("\"%1\"").arg(Folder)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
}

void mlMainWindow::OnOpenConsoleLog()
{
	const QString LogPath = ResolveConsoleLogPath(mGamePath, ActiveFileItem());
	if (LogPath.trimmed().isEmpty())
	{
		QMessageBox::information(this, "Open console_mp.log", "No console log path could be resolved.");
		return;
	}

	if (!QFileInfo(LogPath).isFile())
	{
		QMessageBox::information(this, "Open console_mp.log", QString("console_mp.log was not found.\n\n%1").arg(QDir::toNativeSeparators(LogPath)));
		return;
	}

	ShowTextFileDialog(this, LogPath, "console_mp.log");
}

void mlMainWindow::OnOpenConsoleLogExternal()
{
	const QString LogPath = ResolveConsoleLogPath(mGamePath, ActiveFileItem());
	if (LogPath.trimmed().isEmpty())
	{
		QMessageBox::information(this, "Open console_mp.log", "No console log path could be resolved.");
		return;
	}

	if (!QFileInfo(LogPath).isFile())
	{
		QMessageBox::information(this, "Open console_mp.log", QString("console_mp.log was not found.\n\n%1").arg(QDir::toNativeSeparators(LogPath)));
		return;
	}

	OpenPathWithDefaultApp(LogPath);
}

void mlMainWindow::OnOpenScriptReference()
{
	QDesktopServices::openUrl(QUrl("https://jbird632.github.io/index.html"));
}

void mlMainWindow::OnRunMapOrMod()
{
	QTreeWidgetItem* Item = ActiveFileItem();
	if (!Item)
		return;
	if (ItemCheckState(Item) != Qt::Checked)
	{
		QMessageBox::information(this, "Start", "Check the selected map or mod first to launch it.");
		return;
	}
	QString FsGame;
	QString MapName;

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		MapName = GetItemEntryName(Item);
		TouchRecentEntry(RecentEntryForItem(ML_ITEM_MAP, MapName, MapName));
		FsGame = MapName;
	}
	else
	{
		QString ModName = GetItemContainerName(Item);
		TouchRecentEntry(RecentEntryForItem(Item->data(0, Qt::UserRole).toInt(), ModName, GetItemEntryName(Item)));
		FsGame = ModName;
	}
	QueueGameStatsForItem(GetItemFavoriteKey(Item));

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(CreateGameLaunchCommand(FsGame, MapName));
	StartBuildThread(Commands);
}

void mlMainWindow::OnSaveLog() const
{
	// want to make a logs directory for easy management of launcher logs (exe_dir/logs)
	const auto dir = QDir{};
	if (!dir.exists("logs"))
	{
		const auto result = dir.mkdir("logs");
		if (!result)
		{
			QMessageBox::warning(nullptr, "Error", QString("Could not create the \"logs\" directory"));
			return;
		}
	}

	const auto time = std::time(nullptr);
	auto ss = std::stringstream{};
	const auto timeStr = std::put_time(std::localtime(&time), "%F_%T");

	ss << timeStr;

	auto dateStr = ss.str();
	std::replace(dateStr.begin(), dateStr.end(), ':', '_');

	QFile log(QString("logs/modlog_%1.txt").arg(dateStr.c_str()));

	if (!log.open(QIODevice::WriteOnly))
		return;

	QTextStream stream(&log);
	if (!mOutputFullText.isEmpty())
		stream << mOutputFullText;
	else if (mOutputPlainWidget && mOutputPlainWidget->isVisible())
		stream << mOutputPlainWidget->toPlainText();
	else
	{
		std::function<void(const QTreeWidgetItem*)> WriteItem = [&](const QTreeWidgetItem* Item)
		{
			if (!Item)
				return;

			const QString Line = Item->data(0, ML_LOG_TEXT_ROLE).toString();
			if (!Line.isEmpty())
				stream << Line << "\n";

			for (int ChildIdx = 0; ChildIdx < Item->childCount(); ChildIdx++)
				WriteItem(Item->child(ChildIdx));
		};

		for (int ItemIdx = 0; ItemIdx < mOutputWidget->topLevelItemCount(); ItemIdx++)
			WriteItem(mOutputWidget->topLevelItem(ItemIdx));
	}

	QMessageBox::information(nullptr, QString("Save Log"), QString("The console log has been saved to %1").arg(log.fileName()));
}

void mlMainWindow::OnCleanXPaks()
{
	QTreeWidgetItem* Item = ActiveFileItem();
	if (!Item)
		return;
	QString Folder;

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = GetItemEntryName(Item);
		Folder = QString("%1/usermaps/%2").arg(mGamePath, MapName);
	}
	else
	{
		QString ModName = GetItemContainerName(Item);
		Folder = QString("%1/mods/%2").arg(mGamePath, ModName);
	}

	QString fileListString;
	QStringList fileList;
	QDirIterator it(Folder, QStringList() << "*.xpak", QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext())
	{
		QString filepath = it.next();
		fileList.append(filepath);
		fileListString.append("\n" + QDir(Folder).relativeFilePath(filepath));
	}

	QString relativeFolder = QDir(mGamePath).relativeFilePath(Folder);

	if (fileList.count() == 0)
	{
		QMessageBox::information(this, QString("Clean XPaks (%1)").arg(relativeFolder), QString("There are no XPak's to clean!"));
		return;
	}

	for (int FileIdx = 0; FileIdx < fileList.count(); FileIdx++)
	{
		const QString file = fileList[FileIdx];
		qDebug() << file;
		QFile(file).remove();
	}
}

void mlMainWindow::OnDelete()
{
	QTreeWidgetItem* Item = ActiveFileItem();
	if (!Item)
		return;
	QString Folder;

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = GetItemEntryName(Item);
		Folder = QString("%1/usermaps/%2").arg(mGamePath, MapName);
	}
	else
	{
		QString ModName = GetItemContainerName(Item);
		Folder = QString("%1/mods/%2").arg(mGamePath, ModName);
	}

	if (!ConfirmDestructiveActionTwice(this, "Delete Folder", QString("folder '%1'").arg(QDir::toNativeSeparators(Folder)), "This deletes the folder and all of its contents."))
		return;

	QDir(Folder).removeRecursively();
	PopulateFileList();
}

void mlMainWindow::OnRenameItem()
{
	QTreeWidgetItem* Item = ActiveFileItem();
	if (!Item)
		return;

	const int RawItemType = Item->data(0, Qt::UserRole).toInt();
	if (RawItemType != ML_ITEM_MAP && RawItemType != ML_ITEM_MOD && RawItemType != ML_ITEM_MOD_GROUP)
		return;

	const bool IsMap = (RawItemType == ML_ITEM_MAP);
	const QString CurrentName = IsMap ? GetItemEntryName(Item) : GetItemContainerName(Item);

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle(IsMap ? "Rename Map" : "Rename Mod");
	Dialog.resize(860, 300);
	Dialog.setMinimumSize(800, 280);
	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QLabel* IntroLabel = new QLabel(
		QString("Rename '%1'. This rewrites matching file names and script references, and stores a backup under launcher data before changing anything.")
			.arg(CurrentName),
		&Dialog);
	IntroLabel->setWordWrap(true);
	Layout->addWidget(IntroLabel);

	QFormLayout* FormLayout = new QFormLayout();
	QLineEdit* NameWidget = new QLineEdit(CurrentName, &Dialog);
	NameWidget->setValidator(new QRegularExpressionValidator(QRegularExpression("[a-zA-Z0-9_]*"), &Dialog));
	FormLayout->addRow("New name:", NameWidget);
	Layout->addLayout(FormLayout);

	QCheckBox* DuplicateWidget = new QCheckBox("Duplicate instead of fully rename (keep old)", &Dialog);
	DuplicateWidget->setChecked(true);
	Layout->addWidget(DuplicateWidget);

	QDialogButtonBox* Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	connect(Buttons, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(Buttons, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	Layout->addWidget(Buttons);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	const QString NewName = NameWidget->text().trimmed().toLower();
	if (NewName.isEmpty())
	{
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", "The new name cannot be empty.");
		return;
	}

	if (NewName.compare(CurrentName, Qt::CaseInsensitive) == 0)
	{
		QMessageBox::information(this, IsMap ? "Rename Map" : "Rename Mod", "Choose a different name to rename or duplicate this item.");
		return;
	}

	if (IsMap && mShippedMapList.contains(NewName, Qt::CaseInsensitive))
	{
		QMessageBox::warning(this, "Rename Map", "The new map name cannot match a built-in map.");
		return;
	}

	RenameSelectedItem(Item, NewName, DuplicateWidget->isChecked());
}

bool mlMainWindow::RenameSelectedItem(QTreeWidgetItem* Item, const QString& NewName, bool DuplicateMode)
{
	if (!Item)
		return false;

	const int RawItemType = Item->data(0, Qt::UserRole).toInt();
	const bool IsMap = (RawItemType == ML_ITEM_MAP);
	const int ItemType = IsMap ? ML_ITEM_MAP : ML_ITEM_MOD_GROUP;
	const QString OldName = IsMap ? GetItemEntryName(Item) : GetItemContainerName(Item);
	const QString SourceFolder = IsMap
		? QDir::cleanPath(QString("%1/usermaps/%2").arg(mGamePath, OldName))
		: QDir::cleanPath(QString("%1/mods/%2").arg(mGamePath, OldName));
	const QString TargetFolder = IsMap
		? QDir::cleanPath(QString("%1/usermaps/%2").arg(mGamePath, NewName))
		: QDir::cleanPath(QString("%1/mods/%2").arg(mGamePath, NewName));

	if (!QDir(SourceFolder).exists())
	{
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", QString("The source folder '%1' does not exist.").arg(QDir::toNativeSeparators(SourceFolder)));
		return false;
	}

	if (QFileInfo(TargetFolder).exists())
	{
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", QString("The target folder '%1' already exists.").arg(QDir::toNativeSeparators(TargetFolder)));
		return false;
	}

	QString Error;
	const QString BackupStamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	const QString BackupRoot = QDir::cleanPath(QString("%1/rename_backups/%2/%3_%4")
		.arg(LauncherDataRoot(), IsMap ? "maps" : "mods", OldName, BackupStamp));
	if (!QDir().mkpath(BackupRoot))
	{
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", QString("Unable to create backup folder '%1'.").arg(QDir::toNativeSeparators(BackupRoot)));
		return false;
	}

	if (!CopyDirectoryRecursive(SourceFolder, QDir::cleanPath(QString("%1/%2/%3").arg(BackupRoot, IsMap ? "usermaps" : "mods", OldName)), &Error))
	{
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", Error);
		return false;
	}

	const QString NotesSourcePath = NotesFilePath(ItemType, OldName, OldName);
	const QString NotesTargetPath = NotesFilePath(ItemType, NewName, NewName);
	const QString VersionsSourceRoot = WorkshopVersionsRoot(ItemType, OldName, OldName);
	const QString VersionsTargetRoot = WorkshopVersionsRoot(ItemType, NewName, NewName);
	if (QFileInfo(NotesSourcePath).isFile() && !CopyFilePlain(NotesSourcePath, QDir::cleanPath(QString("%1/launcher_data/%2").arg(BackupRoot, QFileInfo(NotesSourcePath).fileName())), &Error))
	{
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", Error);
		return false;
	}
	if (QDir(VersionsSourceRoot).exists() && !CopyDirectoryRecursive(VersionsSourceRoot, QDir::cleanPath(QString("%1/workshop_versions").arg(BackupRoot)), &Error))
	{
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", Error);
		return false;
	}

	QStringList MapSourceMatches;
	QStringList RawMatches;
	const QString OldPrefix = OldName.left(2).toLower();
	const QString NewPrefix = NewName.left(2).toLower();
	const QString MapSourceRoot = QDir::cleanPath(QString("%1/map_source").arg(mGamePath));
	const QString RawMapsRoot = QDir::cleanPath(QString("%1/raw/maps").arg(mGamePath));
	auto RenamedMapRelativePath = [&](const QString& RootPath, const QString& FilePath) -> QString
	{
		QString RelativePath = QDir(RootPath).relativeFilePath(FilePath);
		if (IsMap && RelativePath.startsWith(OldPrefix + "/", Qt::CaseInsensitive))
			RelativePath = NewPrefix + RelativePath.mid(OldPrefix.length());
		RelativePath.replace(OldName, NewName);
		return RelativePath;
	};

	if (IsMap)
	{
		MapSourceMatches = CollectFilesWhoseRelativePathContains(MapSourceRoot, OldName);
		RawMatches = CollectFilesWhoseRelativePathContains(RawMapsRoot, OldName);

		for (const QString& SourcePath : MapSourceMatches)
		{
			const QString BackupPath = QDir::cleanPath(QString("%1/map_source/%2").arg(BackupRoot, QDir(MapSourceRoot).relativeFilePath(SourcePath)));
			if (!CopyFilePlain(SourcePath, BackupPath, &Error))
			{
				QMessageBox::warning(this, "Rename Map", Error);
				return false;
			}

			const QString TargetPath = QDir::cleanPath(QString("%1/%2").arg(MapSourceRoot, RenamedMapRelativePath(MapSourceRoot, SourcePath)));
			if (QFileInfo(TargetPath).exists())
			{
				QMessageBox::warning(this, "Rename Map", QString("The target file '%1' already exists.").arg(QDir::toNativeSeparators(TargetPath)));
				return false;
			}
		}

		for (const QString& SourcePath : RawMatches)
		{
			const QString BackupPath = QDir::cleanPath(QString("%1/raw/maps/%2").arg(BackupRoot, QDir(RawMapsRoot).relativeFilePath(SourcePath)));
			if (!CopyFilePlain(SourcePath, BackupPath, &Error))
			{
				QMessageBox::warning(this, "Rename Map", Error);
				return false;
			}

			const QString TargetPath = QDir::cleanPath(QString("%1/%2").arg(RawMapsRoot, RenamedMapRelativePath(RawMapsRoot, SourcePath)));
			if (QFileInfo(TargetPath).exists())
			{
				QMessageBox::warning(this, "Rename Map", QString("The target file '%1' already exists.").arg(QDir::toNativeSeparators(TargetPath)));
				return false;
			}
		}
	}

	if (!CopyDirectoryWithRenameTransform(SourceFolder, TargetFolder, OldName, NewName, &Error))
	{
		QDir(TargetFolder).removeRecursively();
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", Error);
		return false;
	}

	if (QFileInfo(NotesSourcePath).isFile() && !CopyFilePlain(NotesSourcePath, NotesTargetPath, &Error))
	{
		QDir(TargetFolder).removeRecursively();
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", Error);
		return false;
	}

	if (QDir(VersionsSourceRoot).exists() && !CopyDirectoryRecursive(VersionsSourceRoot, VersionsTargetRoot, &Error))
	{
		QDir(TargetFolder).removeRecursively();
		QMessageBox::warning(this, IsMap ? "Rename Map" : "Rename Mod", Error);
		return false;
	}

	if (IsMap)
	{
		for (const QString& SourcePath : MapSourceMatches)
		{
			const QString TargetPath = QDir::cleanPath(QString("%1/%2").arg(MapSourceRoot, RenamedMapRelativePath(MapSourceRoot, SourcePath)));
			if (!CopyFileWithRenameTransform(SourcePath, TargetPath, OldName, NewName, &Error))
			{
				QMessageBox::warning(this, "Rename Map", Error);
				return false;
			}
		}

		for (const QString& SourcePath : RawMatches)
		{
			const QString TargetPath = QDir::cleanPath(QString("%1/%2").arg(RawMapsRoot, RenamedMapRelativePath(RawMapsRoot, SourcePath)));
			if (!CopyFileWithRenameTransform(SourcePath, TargetPath, OldName, NewName, &Error))
			{
				QMessageBox::warning(this, "Rename Map", Error);
				return false;
			}
		}
	}

	const QString OldFavoriteKey = RecentEntryForItem(ItemType, OldName, OldName);
	const QString NewFavoriteKey = RecentEntryForItem(ItemType, NewName, NewName);
	QSettings Settings;
	QStringList Favorites = FavoriteEntries();
	for (QString& Entry : Favorites)
	{
		if (!DuplicateMode && Entry.compare(OldFavoriteKey, Qt::CaseInsensitive) == 0)
			Entry = NewFavoriteKey;
	}
	if (DuplicateMode && Favorites.contains(OldFavoriteKey, Qt::CaseInsensitive) && !Favorites.contains(NewFavoriteKey, Qt::CaseInsensitive))
		Favorites.append(NewFavoriteKey);
	{
		QStringList DedupedFavorites;
		QSet<QString> Seen;
		for (const QString& Entry : Favorites)
		{
			const QString Normalized = Entry.trimmed().toLower();
			if (Normalized.isEmpty() || Seen.contains(Normalized))
				continue;
			Seen.insert(Normalized);
			DedupedFavorites << Entry;
		}
		Favorites = DedupedFavorites;
	}
	Settings.setValue("Favorites", Favorites);

	QStringList Recents = RecentEntries();
	for (QString& Entry : Recents)
	{
		if (!DuplicateMode && Entry.compare(OldFavoriteKey, Qt::CaseInsensitive) == 0)
			Entry = NewFavoriteKey;
	}
	if (DuplicateMode)
		Recents.prepend(NewFavoriteKey);
	{
		QStringList DedupedRecents;
		QSet<QString> Seen;
		for (const QString& Entry : Recents)
		{
			const QString Normalized = Entry.trimmed().toLower();
			if (Normalized.isEmpty() || Seen.contains(Normalized))
				continue;
			Seen.insert(Normalized);
			DedupedRecents << Entry;
		}
		Recents = DedupedRecents;
	}
	while (Recents.count() > 12)
		Recents.removeLast();
	Settings.setValue("Recents", Recents);

	const QString OldDisplayName = DisplayNameForEntry(OldFavoriteKey);
	const QString OldDisplayColor = DisplayColorForEntry(OldFavoriteKey);
	if (!OldDisplayName.isEmpty())
		SetDisplayNameForEntry(NewFavoriteKey, OldDisplayName);
	if (!OldDisplayColor.isEmpty())
		SetDisplayColorForEntry(NewFavoriteKey, OldDisplayColor);

	const QString OldStatsKey = NormalizeStatsItemKey(OldFavoriteKey);
	const QString NewStatsKey = NormalizeStatsItemKey(NewFavoriteKey);
	for (const QString& Suffix : QStringList() << "LauncherSeconds" << "RadiantSeconds")
	{
		const QString OldKey = QString("Stats/Items/%1/%2").arg(OldStatsKey, Suffix);
		const QVariant OldValue = Settings.value(OldKey);
		if (OldValue.isValid())
			Settings.setValue(QString("Stats/Items/%1/%2").arg(NewStatsKey, Suffix), OldValue);
	}
	const QVariant PublishCountValue = Settings.value(QString("Stats/ItemsPublishCount/%1").arg(OldStatsKey));
	if (PublishCountValue.isValid())
		Settings.setValue(QString("Stats/ItemsPublishCount/%1").arg(NewStatsKey), PublishCountValue);

	if (!DuplicateMode)
	{
		QDir(SourceFolder).removeRecursively();
		if (QFileInfo(NotesSourcePath).exists())
			QFile::remove(NotesSourcePath);
		if (QDir(VersionsSourceRoot).exists())
			QDir(VersionsSourceRoot).removeRecursively();
		if (IsMap)
		{
			for (const QString& SourcePath : MapSourceMatches)
				QFile::remove(SourcePath);
			for (const QString& SourcePath : RawMatches)
				QFile::remove(SourcePath);
		}

		Settings.remove(QString("DisplayNames/%1").arg(OldFavoriteKey.toLower()));
		Settings.remove(QString("DisplayColors/%1").arg(OldFavoriteKey.toLower()));
		Settings.remove(QString("Stats/Items/%1").arg(OldStatsKey));
		Settings.remove(QString("Stats/ItemsPublishCount/%1").arg(OldStatsKey));
	}

	PopulateFileList();
	QMessageBox::information(this,
		IsMap ? "Rename Map" : "Rename Mod",
		QString("%1 '%2' as '%3'.\n\nBackup saved to:\n%4")
			.arg(DuplicateMode ? "Duplicated" : "Renamed", OldName, NewName, QDir::toNativeSeparators(BackupRoot)));
	return true;
}

void mlMainWindow::OnAnalyzeItem()
{
	QTreeWidgetItem* Item = ActiveFileItem();
	if (!Item)
	{
		QMessageBox::information(this, "Analyze", "Select a map or mod first.");
		return;
	}

	const int ItemType = Item->data(0, Qt::UserRole).toInt();
	if (ItemType == ML_ITEM_UNKNOWN)
	{
		QMessageBox::information(this, "Analyze", "Select a map or mod first.");
		return;
	}

	const bool IsMap = (ItemType == ML_ITEM_MAP);
	const QString TitleName = IsMap ? GetItemEntryName(Item) : GetItemContainerName(Item);
	const QString Prefix = TitleName.left(2).toLower();
	QProgressDialog ProgressDialog(this);
	ProgressDialog.setWindowTitle(QString("Analyze %1").arg(IsMap ? "Map" : "Mod"));
	ProgressDialog.setLabelText(QString("Preparing analysis for %1 '%2'...").arg(IsMap ? "map" : "mod", TitleName));
	ProgressDialog.setCancelButtonText("Cancel");
	ProgressDialog.setRange(0, 8);
	ProgressDialog.setMinimumDuration(0);
	ProgressDialog.setWindowModality(Qt::WindowModal);
	ProgressDialog.setValue(0);
	ProgressDialog.show();
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

	auto UpdateAnalyzeProgress = [&](int Value, const QString& Label) -> bool
	{
		ProgressDialog.setLabelText(Label);
		ProgressDialog.setValue(Value);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		return !ProgressDialog.wasCanceled();
	};

	const QString ContentRoot = IsMap
		? QDir::cleanPath(QString("%1/usermaps/%2").arg(mGamePath, TitleName))
		: QDir::cleanPath(QString("%1/mods/%2").arg(mGamePath, TitleName));
	const QString ScriptsRoot = QDir::cleanPath(QString("%1/scripts").arg(ContentRoot));
	const QString UiRoot = QDir::cleanPath(QString("%1/ui").arg(ContentRoot));
	const QString LuaRoot = QDir::cleanPath(QString("%1/lua").arg(ContentRoot));
	const QString GamedataRoot = QDir::cleanPath(QString("%1/gamedata").arg(ContentRoot));
	const QString ZoneSourceRoot = QDir::cleanPath(QString("%1/zone_source").arg(ContentRoot));
	const QString ZoneOutputRoot = QDir::cleanPath(QString("%1/zone").arg(ContentRoot));

	QStringList Findings;
	QStringList Warnings;
	if (!UpdateAnalyzeProgress(1, QString("Loading defaults for %1 '%2'...").arg(IsMap ? "map" : "mod", TitleName)))
		return;
	QString DefaultScriptsManifestError;
	QString DefaultUiManifestError;
	const QSet<QString> DefaultScriptsManifest = LoadDefaultScriptsManifest(&DefaultScriptsManifestError);
	const QSet<QString> DefaultUiManifest = LoadDefaultUiManifest(&DefaultUiManifestError);
	QString LoadedDefaultScriptsManifestPath = DefaultScriptsManifestPath();
	if (!QFileInfo(LoadedDefaultScriptsManifestPath).isFile())
	{
		if (QFile::exists(":/resources/default_scripts.txt"))
			LoadedDefaultScriptsManifestPath = ":/resources/default_scripts.txt";
		else
			LoadedDefaultScriptsManifestPath = QDir::cleanPath(QString("%1/default_scripts.txt").arg(LauncherDataRoot()));
	}
	const QMultiHash<QString, QString> DefaultScriptNamespaces = LoadDefaultScriptNamespacesFromManifest(LoadedDefaultScriptsManifestPath);
	QHash<QString, QSet<QString>> DefaultNamespacesByScriptPath;
	for (auto It = DefaultScriptNamespaces.constBegin(); It != DefaultScriptNamespaces.constEnd(); ++It)
		DefaultNamespacesByScriptPath[It.value()].insert(It.key());
	auto CheckRequiredFile = [&](const QString& Path, const QString& Label)
	{
		if (QFileInfo(Path).isFile())
			Findings << QString("Found %1: %2").arg(Label, QDir::toNativeSeparators(Path));
		else
			Warnings << QString("Missing %1: %2").arg(Label, QDir::toNativeSeparators(Path));
	};
	auto SharedScriptAssetPath = [](const QString& AssetPath) -> QString
	{
		return NormalizeDefaultManifestPath(QString("share/raw/%1").arg(AssetPath));
	};
	auto GlobalRawPath = [&](const QString& AssetPath) -> QString
	{
		return QDir::cleanPath(QString("%1/raw/%2").arg(mGamePath, AssetPath));
	};
	auto ContentAssetPath = [&](const QString& AssetPath) -> QString
	{
		return QDir::cleanPath(QString("%1/%2").arg(ContentRoot, AssetPath));
	};
	auto AssetSourceLabel = [&](const QString& AssetPath) -> QString
	{
		const QString LocalPath = ContentAssetPath(AssetPath);
		const QString UnsafeGlobalPath = GlobalRawPath(AssetPath);
		const bool IsUiAsset = AssetPath.startsWith("ui/", Qt::CaseInsensitive) || AssetPath.startsWith("lua/", Qt::CaseInsensitive);
		if (QFileInfo(LocalPath).isFile())
			return "local";
		if (QFileInfo(UnsafeGlobalPath).isFile())
			return "unsafe-global";
		if ((IsUiAsset && ManifestContainsUiAsset(DefaultUiManifest, AssetPath))
			|| (!IsUiAsset && DefaultScriptsManifest.contains(SharedScriptAssetPath(AssetPath))))
			return "default";
		return "missing";
	};

	if (!QDir(ContentRoot).exists())
	{
		ProgressDialog.cancel();
		QMessageBox::warning(this, "Analyze", QString("The folder '%1' does not exist.").arg(QDir::toNativeSeparators(ContentRoot)));
		return;
	}
	if (!DefaultScriptsManifestError.isEmpty())
		Warnings << DefaultScriptsManifestError;
	else if (QFileInfo(DefaultScriptsManifestPath()).isFile())
		Findings << QString("Default scripts manifest: %1").arg(QDir::toNativeSeparators(DefaultScriptsManifestPath()));
	else if (QFile::exists(":/resources/default_scripts.txt"))
		Findings << "Default scripts manifest: bundled resource (:/resources/default_scripts.txt)";
	else
		Findings << QString("Default scripts manifest: %1").arg(QDir::toNativeSeparators(QDir::cleanPath(QString("%1/default_scripts.txt").arg(LauncherDataRoot()))));
	if (!DefaultUiManifestError.isEmpty())
		Warnings << DefaultUiManifestError;
	else if (QFileInfo(DefaultUiManifestPath()).isFile())
		Findings << QString("Default UI manifest: %1").arg(QDir::toNativeSeparators(DefaultUiManifestPath()));
	else if (QFile::exists(":/resources/default_ui.txt"))
		Findings << "Default UI manifest: bundled resource (:/resources/default_ui.txt)";
	else
		Findings << QString("Default UI manifest: %1").arg(QDir::toNativeSeparators(QDir::cleanPath(QString("%1/default_ui.txt").arg(LauncherDataRoot()))));

	CheckRequiredFile(QDir::cleanPath(QString("%1/zone/workshop.json").arg(ContentRoot)), "workshop.json");
	if (!UpdateAnalyzeProgress(2, QString("Checking project files for '%1'...").arg(TitleName)))
		return;

	QStringList ZoneFilePaths;
	QStringList PrimaryEntrypoints;
	if (IsMap)
	{
		const QString ZoneFilePath = QDir::cleanPath(QString("%1/%2.zone").arg(ZoneSourceRoot, TitleName));
		ZoneFilePaths << ZoneFilePath;
		PrimaryEntrypoints
			<< QDir::cleanPath(QString("%1/%2/%3.gsc").arg(ScriptsRoot, Prefix, TitleName))
			<< QDir::cleanPath(QString("%1/%2/%3.csc").arg(ScriptsRoot, Prefix, TitleName))
			<< QDir::cleanPath(QString("%1/%2/_load.gsc").arg(ScriptsRoot, Prefix))
			<< QDir::cleanPath(QString("%1/%2/_load.csc").arg(ScriptsRoot, Prefix));

		CheckRequiredFile(QDir::cleanPath(QString("%1/map_source/%2/%3.map").arg(mGamePath, Prefix, TitleName)), "Radiant .map source");
		CheckRequiredFile(QDir::cleanPath(QString("%1/map_source/%2/%3.bak").arg(mGamePath, Prefix, TitleName)), "Radiant .bak source");
		CheckRequiredFile(QDir::cleanPath(QString("%1/sound/zoneconfig/%2.szc").arg(ContentRoot, TitleName)), "sound zone config");
	}
	else
	{
		for (const QString& ZoneName : ModZoneNames(TitleName))
			ZoneFilePaths << QDir::cleanPath(QString("%1/%2.zone").arg(ZoneSourceRoot, ZoneName));
		PrimaryEntrypoints
			<< QDir::cleanPath(QString("%1/%2/_load.gsc").arg(ScriptsRoot, Prefix))
			<< QDir::cleanPath(QString("%1/%2/_load.csc").arg(ScriptsRoot, Prefix))
			<< QDir::cleanPath(QString("%1/scripts/zm/_load.gsc").arg(ContentRoot))
			<< QDir::cleanPath(QString("%1/scripts/zm/_load.csc").arg(ContentRoot))
			<< QDir::cleanPath(QString("%1/scripts/mp/_load.gsc").arg(ContentRoot))
			<< QDir::cleanPath(QString("%1/scripts/mp/_load.csc").arg(ContentRoot));

		if (ZoneFilePaths.isEmpty())
			Warnings << "No mod zone files were found under zone_source.";
	}

	for (const QString& ZoneFilePath : ZoneFilePaths)
		CheckRequiredFile(ZoneFilePath, QString("zone file (%1)").arg(QFileInfo(ZoneFilePath).fileName()));

	bool FoundAnyEntrypoint = false;
	for (const QString& EntryPoint : PrimaryEntrypoints)
	{
		if (QFileInfo(EntryPoint).isFile())
		{
			FoundAnyEntrypoint = true;
			Findings << QString("Found script entrypoint: %1").arg(QDir::toNativeSeparators(EntryPoint));
		}
	}
	if (!FoundAnyEntrypoint)
		Warnings << "No expected script entrypoint files were found.";
	if (!UpdateAnalyzeProgress(3, QString("Scanning scripts for '%1'...").arg(TitleName)))
		return;

	QHash<QString, QString> LocalScripts;
	QHash<QString, QString> LocalUiFiles;
	QHash<QString, QString> LocalLuaFiles;
	QHash<QString, QString> LocalGamedataFiles;
	QHash<QString, QSet<QString>> NamespaceUsagesByScript;
	QHash<QString, QSet<QString>> UsingNamespacesByScript;
	QHash<QString, QSet<QString>> DeclaredNamespacesByScript;
	QHash<QString, QStringList> ScriptUsingsByScript;
	QMultiHash<QString, QString> LocalScriptNamespaces;
	QStringList GscClientfields;
	QStringList CscClientfields;
	QHash<QString, QStringList> GscClientfieldFiles;
	QHash<QString, QStringList> CscClientfieldFiles;
	QStringList ReferencedScriptModules;
	QStringList ReferencedUiModules;
	QMultiHash<QString, QString> ScriptReferenceSources;
	QMultiHash<QString, QString> UiReferenceSources;
	QStringList WeaponTableLoads;
	if (QDir(ScriptsRoot).exists())
	{
		QDirIterator It(ScriptsRoot, QStringList() << "*.gsc" << "*.csc" << "*.gsh", QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
		while (It.hasNext())
		{
			const QString ScriptPath = It.next();
			const QString RelativePath = NormalizeScriptAssetPath(QString("scripts/%1").arg(QDir(ScriptsRoot).relativeFilePath(ScriptPath)));
			LocalScripts.insert(RelativePath, ScriptPath);

			QFile ScriptFile(ScriptPath);
			if (!ScriptFile.open(QIODevice::ReadOnly))
			{
				Warnings << QString("Unable to read script file: %1").arg(QDir::toNativeSeparators(ScriptPath));
				continue;
			}

			const QString Text = QString::fromUtf8(ScriptFile.readAll());
			const QString SanitizedText = StripScriptComments(Text);
			const QStringList ScriptUsings = ExtractScriptUsings(Text);
			ReferencedScriptModules.append(ScriptUsings);
			ScriptUsingsByScript.insert(RelativePath, ScriptUsings);
			const QString CurrentExtension = QFileInfo(RelativePath).suffix().toLower();
			for (const QString& ScriptUsing : ScriptUsings)
			{
				if (CurrentExtension == "gsc")
					ScriptReferenceSources.insert(NormalizeScriptAssetPath(ScriptUsing + ".gsc"), RelativePath);
				else if (CurrentExtension == "csc")
					ScriptReferenceSources.insert(NormalizeScriptAssetPath(ScriptUsing + ".csc"), RelativePath);
				else if (CurrentExtension == "gsh")
					ScriptReferenceSources.insert(NormalizeScriptAssetPath(ScriptUsing + ".gsh"), RelativePath);
				else
					ScriptReferenceSources.insert(ScriptUsing, RelativePath);
			}
			WeaponTableLoads.append(ExtractWeaponTableLoads(SanitizedText));
			QSet<QString> UsingNamespaces;
			for (const QString& UsingPath : ScriptUsings)
			{
				const QString UsingBase = QFileInfo(UsingPath).baseName().toLower();
				if (!UsingBase.isEmpty())
					UsingNamespaces.insert(UsingBase);
				const QString GscUsingPath = NormalizeScriptAssetPath(UsingPath + ".gsc");
				const QString CscUsingPath = NormalizeScriptAssetPath(UsingPath + ".csc");
				const QString GshUsingPath = NormalizeScriptAssetPath(UsingPath + ".gsh");
				for (const QString& NamespaceName : DeclaredNamespacesByScript.value(GscUsingPath))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DeclaredNamespacesByScript.value(CscUsingPath))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DeclaredNamespacesByScript.value(GshUsingPath))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DefaultNamespacesByScriptPath.value(SharedScriptAssetPath(GscUsingPath)))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DefaultNamespacesByScriptPath.value(SharedScriptAssetPath(CscUsingPath)))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DefaultNamespacesByScriptPath.value(SharedScriptAssetPath(GshUsingPath)))
					UsingNamespaces.insert(NamespaceName);
			}
			UsingNamespacesByScript.insert(RelativePath, UsingNamespaces);
			QSet<QString> NamespaceUsages;
			for (const QString& NamespaceName : ExtractNamespaceUsages(SanitizedText))
				NamespaceUsages.insert(NamespaceName);
			NamespaceUsagesByScript.insert(RelativePath, NamespaceUsages);
			QSet<QString> DeclaredNamespaces;
			for (const QString& NamespaceName : ExtractDeclaredNamespaces(SanitizedText))
			{
				LocalScriptNamespaces.insert(NamespaceName, RelativePath);
				DeclaredNamespaces.insert(NamespaceName);
			}
			DeclaredNamespacesByScript.insert(RelativePath, DeclaredNamespaces);
			if (RelativePath.endsWith(".gsc"))
			{
				const QStringList Fields = ExtractClientfieldRegistrations(SanitizedText);
				GscClientfields.append(Fields);
				for (const QString& Field : Fields)
					GscClientfieldFiles[Field].append(ScriptPath);
			}
			if (RelativePath.endsWith(".csc"))
			{
				const QStringList Fields = ExtractClientfieldRegistrations(SanitizedText);
				CscClientfields.append(Fields);
				for (const QString& Field : Fields)
					CscClientfieldFiles[Field].append(ScriptPath);
			}
		}
	}
	if (QDir(UiRoot).exists())
	{
		for (auto UsingIt = ScriptUsingsByScript.constBegin(); UsingIt != ScriptUsingsByScript.constEnd(); ++UsingIt)
		{
			QSet<QString> UsingNamespaces = UsingNamespacesByScript.value(UsingIt.key());
			for (const QString& UsingPath : UsingIt.value())
			{
				const QString GscUsingPath = NormalizeScriptAssetPath(UsingPath + ".gsc");
				const QString CscUsingPath = NormalizeScriptAssetPath(UsingPath + ".csc");
				const QString GshUsingPath = NormalizeScriptAssetPath(UsingPath + ".gsh");
				for (const QString& NamespaceName : DeclaredNamespacesByScript.value(GscUsingPath))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DeclaredNamespacesByScript.value(CscUsingPath))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DeclaredNamespacesByScript.value(GshUsingPath))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DefaultNamespacesByScriptPath.value(SharedScriptAssetPath(GscUsingPath)))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DefaultNamespacesByScriptPath.value(SharedScriptAssetPath(CscUsingPath)))
					UsingNamespaces.insert(NamespaceName);
				for (const QString& NamespaceName : DefaultNamespacesByScriptPath.value(SharedScriptAssetPath(GshUsingPath)))
					UsingNamespaces.insert(NamespaceName);
			}
			UsingNamespacesByScript.insert(UsingIt.key(), UsingNamespaces);
		}

		QDirIterator It(UiRoot, QStringList() << "*.lua" << "*.luac" << "*.menu", QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
		while (It.hasNext())
		{
			const QString FilePath = It.next();
			const QString RelativePath = NormalizeScriptAssetPath(QString("ui/%1").arg(QDir(UiRoot).relativeFilePath(FilePath)));
			LocalUiFiles.insert(RelativePath, FilePath);

			QFile File(FilePath);
			if (!File.open(QIODevice::ReadOnly))
			{
				Warnings << QString("Unable to read UI file: %1").arg(QDir::toNativeSeparators(FilePath));
				continue;
			}

			const QStringList Modules = ExtractUiRequireModules(StripScriptComments(QString::fromUtf8(File.readAll())));
			ReferencedUiModules.append(Modules);
			for (const QString& ModuleName : Modules)
			{
				const QString RawfilePath = LuaModuleToRawfilePath(ModuleName);
				if (!RawfilePath.isEmpty())
					UiReferenceSources.insert(RawfilePath, RelativePath);
			}
		}
	}
	if (QDir(LuaRoot).exists())
	{
		QDirIterator It(LuaRoot, QStringList() << "*.lua" << "*.luac", QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
		while (It.hasNext())
		{
			const QString FilePath = It.next();
			const QString RelativePath = NormalizeScriptAssetPath(QString("lua/%1").arg(QDir(LuaRoot).relativeFilePath(FilePath)));
			LocalLuaFiles.insert(RelativePath, FilePath);
		}
	}
	if (QDir(GamedataRoot).exists())
	{
		QDirIterator It(GamedataRoot, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
		while (It.hasNext())
		{
			const QString FilePath = It.next();
			const QString RelativePath = NormalizeScriptAssetPath(QString("gamedata/%1").arg(QDir(GamedataRoot).relativeFilePath(FilePath)));
			LocalGamedataFiles.insert(RelativePath, FilePath);
		}
	}

	if (LocalScripts.isEmpty())
		Warnings << QString("No local scripts were found under %1.").arg(QDir::toNativeSeparators(ScriptsRoot));
	else
		Findings << QString("Found %1 local script files.").arg(LocalScripts.count());
	if (!LocalUiFiles.isEmpty())
		Findings << QString("Found %1 local UI files.").arg(LocalUiFiles.count());
	if (!LocalLuaFiles.isEmpty())
		Findings << QString("Found %1 local Lua files.").arg(LocalLuaFiles.count());
	if (!LocalGamedataFiles.isEmpty())
		Findings << QString("Found %1 local gamedata files.").arg(LocalGamedataFiles.count());
	if (!UpdateAnalyzeProgress(4, QString("Reading zone files for '%1'...").arg(TitleName)))
		return;

	QSet<QString> ZoneScriptEntries;
	QSet<QString> CommentedZoneScriptEntries;
	QSet<QString> ZoneRawfileEntries;
	QSet<QString> ZoneStringTableEntries;
	QHash<QString, QSet<QString>> ZoneRawfilesByZone;
	bool HasAnyZoneScriptEntries = false;
	for (const QString& ZoneFilePath : ZoneFilePaths)
	{
		QFile ZoneFile(ZoneFilePath);
		if (!ZoneFile.open(QIODevice::ReadOnly))
			continue;

		const QString ZoneText = QString::fromUtf8(ZoneFile.readAll());
		const QString ZoneName = QFileInfo(ZoneFilePath).completeBaseName();
		const QStringList ScriptEntries = ExtractZoneScriptEntries(ZoneText);
		const QStringList CommentedScriptEntries = ExtractCommentedZoneEntriesForType(ZoneText, "scriptparsetree");
		const QStringList RawfileEntries = ExtractZoneEntriesForType(ZoneText, "rawfile");
		const QStringList StringTableEntries = ExtractZoneEntriesForType(ZoneText, "stringtable");
		const QStringList FxEntries = ExtractZoneEntriesForType(ZoneText, "fx");
		const QStringList ImageEntries = ExtractZoneEntriesForType(ZoneText, "image");
		const QStringList XModelEntries = ExtractZoneEntriesForType(ZoneText, "xmodel");

		HasAnyZoneScriptEntries |= !ScriptEntries.isEmpty();
		for (const QString& Entry : ScriptEntries)
			ZoneScriptEntries.insert(Entry);
		for (const QString& Entry : CommentedScriptEntries)
			CommentedZoneScriptEntries.insert(Entry);
		for (const QString& Entry : RawfileEntries)
		{
			ZoneRawfileEntries.insert(Entry);
			ZoneRawfilesByZone[ZoneName].insert(Entry);
		}
		for (const QString& Entry : StringTableEntries)
			ZoneStringTableEntries.insert(Entry);

		Findings << QString("Zone %1 references %2 scripts, %3 rawfiles, %4 stringtables, %5 fx, %6 images, %7 xmodels.")
			.arg(ZoneName)
			.arg(ScriptEntries.count())
			.arg(RawfileEntries.count())
			.arg(StringTableEntries.count())
			.arg(FxEntries.count())
			.arg(ImageEntries.count())
			.arg(XModelEntries.count());
	}
	if (!HasAnyZoneScriptEntries)
		Warnings << "No scriptparsetree entries were found in the available zone files.";
	if (!UpdateAnalyzeProgress(5, QString("Matching custom scripts and assets for '%1'...").arg(TitleName)))
		return;

	for (auto It = LocalScripts.constBegin(); It != LocalScripts.constEnd(); ++It)
	{
		const QString SharedPath = SharedScriptAssetPath(It.key());
		const bool IsSharedDefaultOverride = DefaultScriptsManifest.contains(SharedPath);
		const bool IsCommentedOutInZone = CommentedZoneScriptEntries.contains(It.key());
		if ((It.key().endsWith(".gsc") || It.key().endsWith(".csc")) && !ZoneScriptEntries.contains(It.key()) && !IsSharedDefaultOverride && !IsCommentedOutInZone)
			Warnings << QString("Local script is not listed in any zone file: %1").arg(It.key());
	}

	QSet<QString> MissingScriptModules;
	auto DescribeScriptReferenceSources = [&](const QString& ReferencedAssetPath) -> QString
	{
		QStringList Sources = ScriptReferenceSources.values(ReferencedAssetPath);
		Sources.removeDuplicates();
		Sources.sort();
		return Sources.join(", ");
	};
	for (const QString& ReferencedModule : ReferencedScriptModules)
	{
		const QString GscModule = NormalizeScriptAssetPath(ReferencedModule + ".gsc");
		const QString CscModule = NormalizeScriptAssetPath(ReferencedModule + ".csc");
		const QString GshModule = NormalizeScriptAssetPath(ReferencedModule + ".gsh");
		const bool ExistsLocally = LocalScripts.contains(GscModule) || LocalScripts.contains(CscModule) || LocalScripts.contains(GshModule);
		if (ExistsLocally)
		{
			const bool IsDefaultGsc = DefaultScriptsManifest.contains(SharedScriptAssetPath(GscModule));
			const bool IsDefaultCsc = DefaultScriptsManifest.contains(SharedScriptAssetPath(CscModule));
			const bool IsDefaultGsh = DefaultScriptsManifest.contains(SharedScriptAssetPath(GshModule));
			if (LocalScripts.contains(GscModule) && !ZoneScriptEntries.contains(GscModule) && !IsDefaultGsc)
				Warnings << QString("#using references %1 but it is missing from the zone files (referenced by %2)").arg(GscModule, DescribeScriptReferenceSources(GscModule));
			if (LocalScripts.contains(CscModule) && !ZoneScriptEntries.contains(CscModule) && !IsDefaultCsc)
				Warnings << QString("#using references %1 but it is missing from the zone files (referenced by %2)").arg(CscModule, DescribeScriptReferenceSources(CscModule));
			if (LocalScripts.contains(GshModule) && !ZoneScriptEntries.contains(GshModule) && !IsDefaultGsh)
				Warnings << QString("#using references %1 but it is missing from the zone files (referenced by %2)").arg(GshModule, DescribeScriptReferenceSources(GshModule));
			continue;
		}

		const QString GscSource = AssetSourceLabel(GscModule);
		const QString CscSource = AssetSourceLabel(CscModule);
		const QString GshSource = AssetSourceLabel(GshModule);
		if (GscSource == "unsafe-global" || CscSource == "unsafe-global" || GshSource == "unsafe-global")
			Warnings << QString("#using resolves through global raw/ instead of the map/mod folder or shared defaults: %1").arg(ReferencedModule);
		if (GscSource == "missing" && CscSource == "missing" && GshSource == "missing")
			MissingScriptModules.insert(ReferencedModule);
	}
	for (const QString& MissingModule : MissingScriptModules)
		Warnings << QString("#using points to a script that was not found locally or in shared defaults: %1").arg(MissingModule);
	for (auto It = NamespaceUsagesByScript.constBegin(); It != NamespaceUsagesByScript.constEnd(); ++It)
	{
		const QString ScriptPath = It.key();
		const QSet<QString> UsingNamespaces = UsingNamespacesByScript.value(ScriptPath);
		const QSet<QString> DeclaredNamespaces = DeclaredNamespacesByScript.value(ScriptPath);
		const QString ScriptBaseName = QFileInfo(ScriptPath).baseName().toLower();
		for (const QString& NamespaceName : It.value())
		{
			if (UsingNamespaces.contains(NamespaceName)
				|| (NamespaceName.startsWith("_") && UsingNamespaces.contains(NamespaceName.mid(1)))
				|| (!NamespaceName.startsWith("_") && UsingNamespaces.contains("_" + NamespaceName)))
				continue;
			if (DeclaredNamespaces.contains(NamespaceName)
				|| (NamespaceName.startsWith("_") && DeclaredNamespaces.contains(NamespaceName.mid(1)))
				|| (!NamespaceName.startsWith("_") && DeclaredNamespaces.contains("_" + NamespaceName)))
				continue;
			if (ScriptBaseName == NamespaceName
				|| ScriptBaseName == ("_" + NamespaceName)
				|| (NamespaceName.startsWith("_") && ScriptBaseName == NamespaceName.mid(1)))
				continue;
			if (NamespaceName == "level" || NamespaceName == "self" || NamespaceName == "world" || NamespaceName == "clientfield")
				continue;

			QStringList CandidateNamespaces;
			CandidateNamespaces << NamespaceName;
			if (NamespaceName.startsWith("_"))
				CandidateNamespaces << NamespaceName.mid(1);
			else
				CandidateNamespaces << ("_" + NamespaceName);
			CandidateNamespaces.removeDuplicates();

			bool ResolvedByDefaultNamespace = false;
			for (const QString& CandidateNamespace : CandidateNamespaces)
			{
				if (DefaultScriptNamespaces.contains(CandidateNamespace))
				{
					ResolvedByDefaultNamespace = true;
					break;
				}
			}
			if (ResolvedByDefaultNamespace)
				continue;

			bool NamespaceResolvable = false;
			for (auto LocalIt = LocalScripts.constBegin(); LocalIt != LocalScripts.constEnd(); ++LocalIt)
			{
				if (QFileInfo(LocalIt.key()).baseName().compare(NamespaceName, Qt::CaseInsensitive) == 0)
				{
					NamespaceResolvable = true;
					break;
				}
			}

			if (!NamespaceResolvable)
			{
				for (const QString& CandidateNamespace : CandidateNamespaces)
				{
					if (LocalScriptNamespaces.contains(CandidateNamespace))
					{
						NamespaceResolvable = true;
						break;
					}
					if (DefaultScriptNamespaces.contains(CandidateNamespace))
						continue;

					const QString NamespaceGsc = QString("scripts/zm/%1.gsc").arg(CandidateNamespace);
					const QString NamespaceCsc = QString("scripts/zm/%1.csc").arg(CandidateNamespace);
					const QString GenericNamespaceGsc = QString("scripts/%1.gsc").arg(CandidateNamespace);
					const QString GenericNamespaceCsc = QString("scripts/%1.csc").arg(CandidateNamespace);
					if (AssetSourceLabel(NamespaceGsc) != "missing" || AssetSourceLabel(NamespaceCsc) != "missing"
						|| AssetSourceLabel(GenericNamespaceGsc) != "missing" || AssetSourceLabel(GenericNamespaceCsc) != "missing")
					{
						NamespaceResolvable = true;
						break;
					}
				}
			}

			if (NamespaceResolvable)
				Warnings << QString("Script uses namespace '%1::' without a matching #using in %2").arg(NamespaceName, ScriptPath);
		}
	}
	if (!UpdateAnalyzeProgress(6, QString("Checking UI, clientfields, and build state for '%1'...").arg(TitleName)))
		return;

	for (const QString& ModuleName : ReferencedUiModules)
	{
		const QString RawfilePath = LuaModuleToRawfilePath(ModuleName);
		if (RawfilePath.isEmpty())
			continue;

		QStringList ParentSources = UiReferenceSources.values(RawfilePath);
		ParentSources.removeDuplicates();
		bool HasZonedParentSource = ParentSources.isEmpty();
		for (const QString& ParentSource : ParentSources)
		{
			if (ZoneContainsRawfilePath(ZoneRawfileEntries, ParentSource))
			{
				HasZonedParentSource = true;
				break;
			}
		}
		if (!HasZonedParentSource)
			continue;

		const QString SourceLabel = AssetSourceLabel(RawfilePath);
		if (HashContainsPathVariant(LocalUiFiles, RawfilePath) || HashContainsPathVariant(LocalLuaFiles, RawfilePath))
		{
			if (!ZoneContainsRawfilePath(ZoneRawfileEntries, RawfilePath))
				Warnings << QString("Custom UI/Lua file is required but not added as rawfile in a zone: %1").arg(RawfilePath);
		}
		else if (SourceLabel == "unsafe-global")
		{
			Warnings << QString("UI/Lua module is being loaded from global raw/ instead of the map/mod folder or shared defaults: %1").arg(RawfilePath);
		}
		else if (SourceLabel == "missing")
		{
			Warnings << QString("Required UI/Lua module was not found locally or in shared defaults: %1").arg(RawfilePath);
		}
	}

	QSet<QString> GscClientfieldSet;
	QSet<QString> CscClientfieldSet;
	for (const QString& Field : GscClientfields)
		GscClientfieldSet.insert(Field);
	for (const QString& Field : CscClientfields)
		CscClientfieldSet.insert(Field);
	if (!GscClientfieldSet.isEmpty() || !CscClientfieldSet.isEmpty())
		Findings << QString("Clientfield registrations found: %1 in .gsc, %2 in .csc").arg(GscClientfieldSet.count()).arg(CscClientfieldSet.count());

	auto MakeSelectLink = [](const QString& FilePath) -> QString
	{
		return QString(" [[select:%1|Open folder]]").arg(QDir::toNativeSeparators(QFileInfo(FilePath).absolutePath()));
	};
	auto DescribeClientfieldFiles = [&](const QHash<QString, QStringList>& FieldFiles, const QString& FieldName) -> QString
	{
		QStringList Details;
		const QStringList Files = FieldFiles.value(FieldName);
		for (const QString& FilePath : Files)
			Details << QString("%1%2").arg(QFileInfo(FilePath).fileName(), MakeSelectLink(FilePath));
		return Details.join(", ");
	};

	for (const QString& Field : GscClientfieldSet)
	{
		if (!CscClientfieldSet.contains(Field))
			Warnings << QString("Clientfield registered in .gsc but not .csc: %1 (%2)").arg(Field, DescribeClientfieldFiles(GscClientfieldFiles, Field));
	}
	for (const QString& Field : CscClientfieldSet)
	{
		if (!GscClientfieldSet.contains(Field))
			Warnings << QString("Clientfield registered in .csc but not .gsc: %1 (%2)").arg(Field, DescribeClientfieldFiles(CscClientfieldFiles, Field));
	}

	const QStringList BuiltLanguages = DetectBuiltLanguages(ContentRoot);
	if (BuiltLanguages.isEmpty())
		Warnings << "No built localized output folders were found.";
	else if (HasOnlyEnglishBuild(ContentRoot))
	{
		Findings << QString("Built language folders: %1").arg(BuiltLanguages.join(", "));
		Warnings << "Only english is built. Build the rest of the localized languages too.";
	}
	else
		Findings << QString("Built language folders: %1").arg(BuiltLanguages.join(", "));

	if (QDir(ZoneOutputRoot).exists())
	{
		QStringList BuiltFastfiles = QDir(ZoneOutputRoot).entryList(QStringList() << "*.ff", QDir::Files);
		QStringList BuiltXpaks = QDir(ZoneOutputRoot).entryList(QStringList() << "*.xpak", QDir::Files);
		Findings << QString("Built outputs in zone/: %1 fastfiles, %2 xpaks").arg(BuiltFastfiles.count()).arg(BuiltXpaks.count());
		if (BuiltFastfiles.isEmpty())
			Warnings << "No .ff files were found in the zone output folder.";
	}
	else
	{
		Warnings << QString("Missing zone output folder: %1").arg(QDir::toNativeSeparators(ZoneOutputRoot));
	}

	if (!ZoneRawfileEntries.isEmpty())
	{
		for (const QString& RawfileEntry : ZoneRawfileEntries)
		{
			if (RawfileEntry.endsWith(".cfg", Qt::CaseInsensitive))
				continue;
			const QString SourceLabel = AssetSourceLabel(RawfileEntry);
			if (SourceLabel == "missing")
				Warnings << QString("Zone references missing rawfile: %1").arg(RawfileEntry);
			else if (SourceLabel == "unsafe-global")
				Warnings << QString("Zone rawfile resolves through global raw/ instead of the map/mod folder or shared defaults: %1").arg(RawfileEntry);
		}
	}

	for (auto It = LocalUiFiles.constBegin(); It != LocalUiFiles.constEnd(); ++It)
	{
		if (!ZoneContainsRawfilePath(ZoneRawfileEntries, It.key()))
		{
			if (ManifestContainsUiAsset(DefaultUiManifest, It.key()))
				Warnings << QString("Local UI override exists in the project but appears unused because it is not zoned. Remove it if unused: %1").arg(It.key());
			else
				Warnings << QString("Custom UI file exists in the project but is not added as a rawfile in any zone: %1").arg(It.key());
		}
	}
	for (auto It = LocalGamedataFiles.constBegin(); It != LocalGamedataFiles.constEnd(); ++It)
	{
		if (It.key().endsWith(".csv") && !ZoneStringTableEntries.contains(It.key()) && !ZoneRawfileEntries.contains(It.key()))
			Warnings << QString("Local gamedata file exists in the project but appears unused because it is not zoned: %1").arg(It.key());
	}

	bool UsesDefaultLevelcommonName = false;
	for (const QString& WeaponTablePath : WeaponTableLoads)
	{
		if (WeaponTablePath == "gamedata/weapons/zm/zm_levelcommon_weapons.csv")
		{
			UsesDefaultLevelcommonName = true;
			Warnings << "Weapon table load uses the default name 'gamedata/weapons/zm/zm_levelcommon_weapons.csv'. Use a custom file name instead.";
		}
		else if (WeaponTablePath.startsWith("gamedata/weapons/zm/") && QFileInfo(ContentAssetPath(WeaponTablePath)).isFile())
			Findings << QString("Custom weapon table load found: %1").arg(WeaponTablePath);
	}
	if (ZoneStringTableEntries.contains("gamedata/weapons/zm/zm_levelcommon_weapons.csv"))
	{
		UsesDefaultLevelcommonName = true;
		Warnings << "Zone file includes the default 'gamedata/weapons/zm/zm_levelcommon_weapons.csv'. Use a custom weapon table file name instead.";
	}
	if (!UsesDefaultLevelcommonName)
	{
		bool HasCustomWeaponTable = false;
		for (const QString& WeaponTablePath : WeaponTableLoads)
		{
			if (WeaponTablePath.startsWith("gamedata/weapons/zm/") && WeaponTablePath != "gamedata/weapons/zm/zm_levelcommon_weapons.csv")
				HasCustomWeaponTable = true;
		}
		if (HasCustomWeaponTable)
			Findings << "Weapon table usage avoids the default zm_levelcommon_weapons name.";
	}
	if (!UpdateAnalyzeProgress(7, QString("Checking zone placement rules for '%1'...").arg(TitleName)))
		return;

	if (!IsMap)
	{
		const bool HasCoreZone = ZoneRawfilesByZone.contains("core_mod");
		const bool HasZmZone = ZoneRawfilesByZone.contains("zm_mod");
		if (HasCoreZone && HasZmZone)
		{
			for (auto It = LocalUiFiles.constBegin(); It != LocalUiFiles.constEnd(); ++It)
			{
				const QString Path = It.key();
				const bool InCore = ZoneContainsRawfilePath(ZoneRawfilesByZone.value("core_mod"), Path);
				const bool InZm = ZoneContainsRawfilePath(ZoneRawfilesByZone.value("zm_mod"), Path);
				const QString LowerPath = Path.toLower();
				const bool LooksLikeLobbyFrontend = LowerPath.contains("/lobby/") || LowerPath.contains("frontend");
				const bool LooksLikePauseMenu = LowerPath.contains("startmenu") || LowerPath.contains("pause") || LowerPath.contains("saveandquit");
				if (LooksLikeLobbyFrontend && !LooksLikePauseMenu && InZm && !InCore)
					Warnings << QString("Lobby/frontend UI file is only in zm_mod. It likely belongs in core_mod: %1").arg(Path);
				if ((LowerPath.contains("/hud/") || LowerPath.contains("zmhud") || LowerPath.contains("waypoint")) && InCore && !InZm)
					Warnings << QString("In-game HUD UI file is only in core_mod. It likely belongs in zm_mod: %1").arg(Path);
			}
		}
	}

	if (Warnings.isEmpty())
		Findings << QString("No obvious %1 issues were found.").arg(IsMap ? "map" : "mod");
	if (!UpdateAnalyzeProgress(8, QString("Finalizing analysis for '%1'...").arg(TitleName)))
		return;

	ProgressDialog.close();
	ShowAnalysisResults(QString("Analyze %1: %2").arg(IsMap ? "Map" : "Mod", TitleName), Findings, Warnings);
}

void mlMainWindow::ShowAnalysisResults(const QString& Title, const QStringList& Findings, const QStringList& Warnings) const
{
	QDialog Dialog(const_cast<mlMainWindow*>(this), Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle(Title);
	Dialog.resize(920, 700);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QTextBrowser* Output = new QTextBrowser(&Dialog);
	Output->setReadOnly(true);
	Output->setOpenLinks(false);
	Output->setOpenExternalLinks(false);
	Output->setStyleSheet("QTextBrowser { background:#171b20; color:#d7dce2; border:1px solid #2d353d; border-radius:12px; padding:8px; }");

	QStringList SuccessLines;
	QStringList WarningLines;
	QStringList ErrorLines;
	auto IsErrorLine = [](const QString& Line) -> bool
	{
		const QString Lower = Line.toLower();
		return Lower.startsWith("missing ")
			|| Lower.contains(" was not found")
			|| Lower.contains("missing from")
			|| Lower.contains("unsafe-global")
			|| Lower.contains("global raw/")
			|| Lower.contains("unable to")
			|| Lower.contains("not added")
			|| Lower.contains("not listed")
			|| Lower.contains("uses the default name")
			|| Lower.contains("includes the default")
			|| Lower.contains("does not exist");
	};
	auto IsWarningLine = [](const QString& Line) -> bool
	{
		const QString Lower = Line.toLower();
		return Lower.contains("likely belongs")
			|| Lower.contains("clientfield")
			|| Lower.contains("no built")
			|| Lower.contains("no expected")
			|| Lower.contains("no local")
			|| Lower.contains("no scriptparsetree")
			|| Lower.contains("only in ");
	};

	for (const QString& Line : Findings)
	{
		if (!Line.trimmed().isEmpty())
			SuccessLines << Line;
	}
	for (const QString& Line : Warnings)
	{
		if (Line.trimmed().isEmpty())
			continue;
		if (IsErrorLine(Line))
			ErrorLines << Line;
		else if (IsWarningLine(Line))
			WarningLines << Line;
		else
			WarningLines << Line;
	}

	auto HtmlEscape = [](const QString& Text) -> QString { return Text.toHtmlEscaped(); };
	auto FormatAnalysisLine = [&](const QString& Line) -> QString
	{
		QString Html = Line.toHtmlEscaped();
		QRegularExpression LinkExpression("\\[\\[select:([^\\|\\]]+)\\|([^\\]]+)\\]\\]");
		QRegularExpressionMatchIterator It = LinkExpression.globalMatch(Html);
		while (It.hasNext())
		{
			const QRegularExpressionMatch Match = It.next();
			const QString FilePath = Match.captured(1);
			const QString Label = Match.captured(2);
			const QString LinkHtml = QString("<a href=\"select:%1\" style=\"color:#8fc7ff;\">%2</a>")
				.arg(QUrl::toPercentEncoding(FilePath))
				.arg(Label.toHtmlEscaped());
			Html.replace(Match.captured(0), LinkHtml);
		}
		return Html;
	};
	auto BuildSection = [&](const QString& Label, const QString& BadgeText, const QString& BadgeColor, const QString& BackgroundColor, const QStringList& Lines) -> QString
	{
		QString Html = QString(
			"<div style='margin:0 0 14px 0; padding:14px 16px; border-radius:12px; background:%1; border:1px solid %2;'>"
			"<div style='display:flex; justify-content:space-between; align-items:center; margin-bottom:10px;'>"
			"<span style='font-size:15px; font-weight:700; color:#f4f7fb;'>%3</span>"
			"<span style='background:%2; color:#0f1318; border-radius:999px; padding:3px 10px; font-weight:700;'>%4</span>"
			"</div>")
			.arg(BackgroundColor, BadgeColor, HtmlEscape(Label), HtmlEscape(BadgeText));

		if (Lines.isEmpty())
		{
			Html += "<div style='color:#97a6b5;'>None</div></div>";
			return Html;
		}

		Html += "<ul style='margin:0; padding-left:18px;'>";
		for (const QString& Line : Lines)
			Html += QString("<li style='margin:0 0 6px 0; color:#dbe3ea;'>%1</li>").arg(FormatAnalysisLine(Line));
		Html += "</ul></div>";
		return Html;
	};

	QString Html;
	Html += "<html><body style='font-family:Segoe UI; background:#11161b; color:#d7dce2;'>";
	Html += QString(
		"<div style='margin-bottom:20px; padding:18px 20px; border-radius:12px; background:#1b222a; border:1px solid #2d353d;'>"
		"<div style='font-size:18px; font-weight:700; color:#ffffff; margin-bottom:14px;'>Checks completed</div>"
		"<div style='color:#9aa8b6;'>"
		"<span style='display:inline-block; margin-right:14px; margin-bottom:8px; background:#1f5f3b; color:#d9ffe8; border-radius:999px; padding:6px 12px; font-weight:700;'>Success %1</span>"
		"<span style='display:inline-block; margin-right:14px; margin-bottom:8px; background:#6c5520; color:#ffe9b0; border-radius:999px; padding:6px 12px; font-weight:700;'>Warnings %2</span>"
		"<span style='display:inline-block; margin-bottom:8px; background:#6b2626; color:#ffd0d0; border-radius:999px; padding:6px 12px; font-weight:700;'>Errors %3</span>"
		"</div></div>")
		.arg(SuccessLines.count())
		.arg(WarningLines.count())
		.arg(ErrorLines.count());
	Html += BuildSection("Success / Info", QString::number(SuccessLines.count()), "#2f9e62", "#17261d", SuccessLines);
	Html += BuildSection("Warnings", QString::number(WarningLines.count()), "#d6a23d", "#2c2412", WarningLines);
	Html += BuildSection("Errors", QString::number(ErrorLines.count()), "#d85c5c", "#2b1717", ErrorLines);
	Html += "</body></html>";

	Output->setHtml(Html);
	connect(Output, &QTextBrowser::anchorClicked, &Dialog, [&](const QUrl& Url)
	{
		if (Url.scheme() != "select")
			return;

		QString FilePath = Url.toString();
		if (FilePath.startsWith("select:"))
			FilePath.remove(0, QString("select:").length());
		FilePath = QUrl::fromPercentEncoding(FilePath.toUtf8());
		if (FilePath.startsWith(":"))
			FilePath.remove(0, 1);
		if (QFileInfo(FilePath).exists())
			QProcess::startDetached("explorer.exe", QStringList() << QDir::toNativeSeparators(FilePath));
	});
	Layout->addWidget(Output, 1);

	QDialogButtonBox* Buttons = new QDialogButtonBox(QDialogButtonBox::Close, &Dialog);
	connect(Buttons, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	Layout->addWidget(Buttons);
	Dialog.exec();
}

void mlMainWindow::OnExport2BinChooseDirectory()
{
	const QString dir = QFileDialog::getExistingDirectory(mExport2BinGUIWidget, tr("Open Directory"), mToolsPath, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	this->mExport2BinTargetDirWidget->setText(dir);

	QSettings Settings;
	Settings.setValue("Export2Bin_TargetDir", dir);
}

void mlMainWindow::OnExport2BinToggleOverwriteFiles()
{
	QSettings Settings;
	Settings.setValue("Export2Bin_OverwriteFiles", mExport2BinOverwriteWidget->isChecked());
}

void mlMainWindow::OnToggleFavorite()
{
	QTreeWidgetItem* Item = ActiveFileItem();
	if (!Item)
		return;

	ToggleFavoriteEntry(GetItemFavoriteKey(Item));
	PopulateFileList();
}

void mlMainWindow::BuildOutputReady(QString Output)
{
	if (Output.isEmpty())
		return;

	mPendingOutput += Output;
	mOutputFullText += Output;
	if (!mOutputFlushTimer.isActive())
		mOutputFlushTimer.start(16);
}

void mlMainWindow::FlushBuildOutput()
{
	QString Output = mPendingOutput;
	mPendingOutput.clear();
	if (Output.isEmpty())
		return;

	if (!mOutputWidget)
		return;

	QSettings Settings;
	const bool ImprovedConsole = UseImprovedConsoleStyle(Settings);
	const bool ShowPlainConsole = true;
	const bool CompactPlainSpacing = ImprovedConsole;
	if (!ImprovedConsole)
	{
		if (!mOutputPlainWidget)
			return;

		QScrollBar* PlainScrollBar = mOutputPlainWidget->verticalScrollBar();
		const bool WasAtBottom = mOutputPlainAutoFollow;
		const int PreviousScrollValue = PlainScrollBar ? PlainScrollBar->value() : 0;
		QString OriginalOutput = BuildPlainConsoleOutput(Output, false, OutputLogTabFull);
		if (OriginalOutput.isEmpty())
			return;
		mOutputPlainWidget->moveCursor(QTextCursor::End);
		mOutputPlainWidget->insertPlainText(OriginalOutput);
		if (PlainScrollBar)
		{
			if (WasAtBottom)
				PlainScrollBar->setValue(PlainScrollBar->maximum());
			else
				PlainScrollBar->setValue(qMin(PreviousScrollValue, PlainScrollBar->maximum()));
		}
		return;
	}

	if (mOutputTabIndex != OutputLogTabFull)
	{
		RebuildOutputFromBuffer();
		return;
	}

	if (mOutputPlainWidget)
	{
		if (ShowPlainConsole && CompactPlainSpacing)
		{
			QScrollBar* PlainScrollBar = mOutputPlainWidget->verticalScrollBar();
			const bool PlainWasAtBottom = mOutputPlainAutoFollow;
			const int PreviousPlainScrollValue = PlainScrollBar ? PlainScrollBar->value() : 0;
			mOutputPlainWidget->setPlainText(BuildPlainConsoleOutput(mOutputFullText, true, mOutputTabIndex));
			if (PlainScrollBar)
			{
				if (PlainWasAtBottom)
					PlainScrollBar->setValue(PlainScrollBar->maximum());
				else
					PlainScrollBar->setValue(qMin(PreviousPlainScrollValue, PlainScrollBar->maximum()));
			}
		}
		else
		{
			const QString PlainOutput = BuildPlainConsoleOutput(Output, false, mOutputTabIndex);
			if (!PlainOutput.trimmed().isEmpty())
			{
				QScrollBar* PlainScrollBar = mOutputPlainWidget->verticalScrollBar();
				const bool PlainWasAtBottom = mOutputPlainAutoFollow;
				const int PreviousPlainScrollValue = PlainScrollBar ? PlainScrollBar->value() : 0;
				mOutputPlainWidget->moveCursor(QTextCursor::End);
				mOutputPlainWidget->insertPlainText(PlainOutput);
				if (PlainScrollBar)
				{
					if (PlainWasAtBottom)
						PlainScrollBar->setValue(PlainScrollBar->maximum());
					else
						PlainScrollBar->setValue(qMin(PreviousPlainScrollValue, PlainScrollBar->maximum()));
				}
			}
		}
	}

	if (ShowPlainConsole)
		return;

	QScrollBar* ScrollBar = mOutputWidget->verticalScrollBar();
	const bool WasAtBottom = mOutputTreeAutoFollow;
	const int PreviousScrollValue = ScrollBar ? ScrollBar->value() : 0;
	const bool SliderWasActive = ScrollBar && ScrollBar->isSliderDown();
	QString NormalizedOutput = Output.replace("\r", "");
	if (NormalizedOutput.trimmed().isEmpty())
		return;

	QStringList OutputLines = NormalizedOutput.split('\n');
	mOutputWidget->setUpdatesEnabled(false);

	for (int LineIdx = 0; LineIdx < OutputLines.count(); LineIdx++)
	{
		const QString RawLine = OutputLines[LineIdx];
		const QString TrimmedLine = RawLine.trimmed();
		if (TrimmedLine.isEmpty())
			continue;

		if (!ShouldDisplayLogOutput(Settings, TrimmedLine, mOutputTabIndex))
			continue;

		const bool CurrentBlockIsUnrecoverable = mCurrentOutputBlockItem && IsUnrecoverableLogLine(mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString());
		const bool ContinueUnrecoverableBlock = CurrentBlockIsUnrecoverable && IsUnrecoverableLogLine(TrimmedLine);
		const bool ContinueCurrentBlock = mCurrentOutputBlockItem && ShouldContinueCurrentLogBlock(mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString(), TrimmedLine);
		const bool StartsNewBlock = !ContinueUnrecoverableBlock && !ContinueCurrentBlock && (!ImprovedConsole || !mCurrentOutputBlockItem || IsLogBlockHeader(TrimmedLine) || LogDetailIndentLevel(RawLine) <= 0);
		LogMessageKind MessageKind = DetectLogMessageKind(TrimmedLine);
		if (IsUnrecoverableLogLine(TrimmedLine))
			MessageKind = LogMessageError;
		Q_UNUSED(MessageKind);
		const QColor OutputColor = ColorForLogMessageKind(Settings, LogMessageDefault);

		if (StartsNewBlock)
		{
			if (mOutputWidget->topLevelItemCount() > 0)
			{
				QTreeWidgetItem* SpacerItem = new QTreeWidgetItem(mOutputWidget);
				SpacerItem->setData(0, ML_LOG_TEXT_ROLE, QString());
				SpacerItem->setData(0, ML_LOG_IS_HEADER_ROLE, false);
				SpacerItem->setFlags(SpacerItem->flags() & ~Qt::ItemIsSelectable);
				SpacerItem->setSizeHint(0, QSize(0, 0));
				QWidget* SpacerWidget = new QWidget(mOutputWidget);
				SpacerWidget->setStyleSheet("background: transparent;");
				mOutputWidget->setItemWidget(SpacerItem, 0, SpacerWidget);
			}

			mCurrentOutputBlockItem = new QTreeWidgetItem(mOutputWidget);
			mCurrentOutputBlockItem->setData(0, ML_LOG_TEXT_ROLE, ImprovedConsole ? TrimmedLine : RawLine);
			mCurrentOutputBlockItem->setData(0, ML_LOG_IS_HEADER_ROLE, true);
			mCurrentOutputBlockItem->setData(0, ML_LOG_EXPANDED_ROLE, ImprovedConsole);
			mCurrentOutputBlockItem->setData(0, ML_LOG_TEXT_COLOR_ROLE, OutputColor);
			mCurrentOutputBlockItem->setSizeHint(0, QSize(0, LogBlockWidgetHeight(mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString(), ImprovedConsole)));
			mCurrentOutputBlockItem->setExpanded(ImprovedConsole);

			const QColor BackgroundColor = ImprovedConsole
				? OutputBlockBackgroundColor(mThemeMode, LogMessageDefault, mOutputBlockCounter).lighter(125)
				: QColor(0, 0, 0, 0);
			mOutputBlockCounter++;
			mCurrentOutputBlockItem->setData(0, ML_LOG_BG_COLOR_ROLE, BackgroundColor);
			mOutputWidget->setItemWidget(mCurrentOutputBlockItem, 0, CreateLogBlockWidget(mOutputWidget, mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString(), OutputColor, BackgroundColor, ImprovedConsole));
			mOutputWidget->setItemWidget(mCurrentOutputBlockItem, 1, CreateLogBlockActionWidget(mOutputWidget, mCurrentOutputBlockItem));

			Q_UNUSED(ImprovedConsole);
			continue;
		}

		QString RenderedLine = RawLine;
		if (RenderedLine.trimmed() == TrimmedLine && LogDetailIndentLevel(RawLine) > 0)
			RenderedLine = QString(LogDetailIndentLevel(RawLine) * 4, ' ') + TrimmedLine;
		QString BlockText = mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString();
		if (!BlockText.isEmpty())
			BlockText += "\n";
		BlockText += ImprovedConsole ? RenderedLine : RawLine;
		mCurrentOutputBlockItem->setData(0, ML_LOG_TEXT_ROLE, BlockText);
		mCurrentOutputBlockItem->setSizeHint(0, QSize(0, LogBlockWidgetHeight(BlockText, mCurrentOutputBlockItem->data(0, ML_LOG_EXPANDED_ROLE).toBool())));
		LogMessageKind HeaderKind = DetectLogMessageKind(BlockText);
		if (IsUnrecoverableLogLine(BlockText))
			HeaderKind = LogMessageError;
		Q_UNUSED(HeaderKind);
		const QColor BackgroundColor = QColor(0, 0, 0, 0);
		const QColor HeaderColor = ColorForLogMessageKind(Settings, LogMessageDefault);
		mCurrentOutputBlockItem->setData(0, ML_LOG_TEXT_COLOR_ROLE, HeaderColor);
		mCurrentOutputBlockItem->setData(0, ML_LOG_BG_COLOR_ROLE, BackgroundColor);
		UpdateLogBlockWidget(mOutputWidget->itemWidget(mCurrentOutputBlockItem, 0), BlockText, HeaderColor, BackgroundColor, mCurrentOutputBlockItem->data(0, ML_LOG_EXPANDED_ROLE).toBool());
	}
	mOutputWidget->setUpdatesEnabled(true);

	if (ScrollBar)
	{
		if (WasAtBottom && !SliderWasActive)
			ScrollBar->setValue(ScrollBar->maximum());
		else if (!SliderWasActive)
			ScrollBar->setValue(qMin(PreviousScrollValue, ScrollBar->maximum()));
	}
}

void mlMainWindow::BuildFinished()
{
	FlushBuildOutput();
	mlBuildThread* FinishedBuildThread = mBuildThread;
	mlConvertThread* FinishedConvertThread = mConvertThread;
	mBuildThread = NULL;
	mConvertThread = NULL;
	ResetBuildButtons();
	UpdateGameRunningState();

	if (FinishedBuildThread)
		FinishedBuildThread->deleteLater();

	if (FinishedConvertThread)
		FinishedConvertThread->deleteLater();
}

void mlMainWindow::AppendOutputBlock(const QString& BlockText, const QSettings& Settings)
{
	if (!mOutputWidget || BlockText.trimmed().isEmpty())
		return;

	if (mOutputWidget->topLevelItemCount() > 0)
	{
		QTreeWidgetItem* SpacerItem = new QTreeWidgetItem(mOutputWidget);
		SpacerItem->setData(0, ML_LOG_TEXT_ROLE, QString());
		SpacerItem->setData(0, ML_LOG_IS_HEADER_ROLE, false);
		SpacerItem->setFlags(SpacerItem->flags() & ~Qt::ItemIsSelectable);
		SpacerItem->setSizeHint(0, QSize(0, 0));
		QWidget* SpacerWidget = new QWidget(mOutputWidget);
		SpacerWidget->setStyleSheet("background: transparent;");
		mOutputWidget->setItemWidget(SpacerItem, 0, SpacerWidget);
	}

	LogMessageKind HeaderKind = DetectLogMessageKind(BlockText);
	if (IsUnrecoverableLogLine(BlockText))
		HeaderKind = LogMessageError;
	Q_UNUSED(HeaderKind);
	const QColor OutputColor = ColorForLogMessageKind(Settings, LogMessageDefault);
	const QColor BackgroundColor = OutputBlockBackgroundColor(mThemeMode, LogMessageDefault, mOutputBlockCounter).lighter(125);
	mOutputBlockCounter++;

	mCurrentOutputBlockItem = new QTreeWidgetItem(mOutputWidget);
	mCurrentOutputBlockItem->setData(0, ML_LOG_TEXT_ROLE, BlockText);
	mCurrentOutputBlockItem->setData(0, ML_LOG_IS_HEADER_ROLE, true);
	mCurrentOutputBlockItem->setData(0, ML_LOG_EXPANDED_ROLE, true);
	mCurrentOutputBlockItem->setData(0, ML_LOG_TEXT_COLOR_ROLE, OutputColor);
	mCurrentOutputBlockItem->setData(0, ML_LOG_BG_COLOR_ROLE, BackgroundColor);
	mCurrentOutputBlockItem->setSizeHint(0, QSize(0, LogBlockWidgetHeight(BlockText, true)));
	mCurrentOutputBlockItem->setExpanded(true);
	mOutputWidget->setItemWidget(mCurrentOutputBlockItem, 0, CreateLogBlockWidget(mOutputWidget, BlockText, OutputColor, BackgroundColor, true));
	mOutputWidget->setItemWidget(mCurrentOutputBlockItem, 1, CreateLogBlockActionWidget(mOutputWidget, mCurrentOutputBlockItem));

	Q_UNUSED(BackgroundColor);
}

void mlMainWindow::UpdateOutputConsoleMode()
{
	QSettings Settings;
	const bool ImprovedConsole = UseImprovedConsoleStyle(Settings);
	const bool ColorizePlainConsole = ImprovedConsole && IsConsoleColorCodingEnabled(Settings);
	const QColor DefaultPlainColor = ColorForLogMessageKind(Settings, LogMessageDefault);
	EnsureOutputPlainHighlighter(mOutputPlainWidget, ColorizePlainConsole, DefaultPlainColor);
	if (mOutputTabs)
		mOutputTabs->setVisible(ImprovedConsole);
	if (mOutputWidget)
		mOutputWidget->setVisible(false);
	if (mOutputPlainWidget)
	{
		mOutputPlainWidget->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		mOutputPlainWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		mOutputPlainWidget->setVisible(true);
	}
}

void mlMainWindow::RebuildOutputFromBuffer()
{
	if (!mOutputWidget)
		return;

	QSettings Settings;
	const bool ImprovedConsole = UseImprovedConsoleStyle(Settings);
	const QString FullOutput = mOutputFullText;
	if (mOutputWidget)
		mOutputWidget->clear();
	if (mOutputPlainWidget)
		mOutputPlainWidget->clear();
	mCurrentOutputBlockItem = NULL;
	mOutputBlockCounter = 0;
	mOutputTreeAutoFollow = true;
	mOutputPlainAutoFollow = true;
	if (FullOutput.isEmpty())
		return;

	QStringList VisibleBlocks;
	for (const QString& BlockText : ExtractLogBlocks(FullOutput))
	{
		if (ShouldDisplayLogBlock(Settings, BlockText, mOutputTabIndex))
			VisibleBlocks.append(BlockText);
	}

	if (mOutputPlainWidget)
		mOutputPlainWidget->setPlainText(BuildPlainConsoleOutput(FullOutput, ImprovedConsole, mOutputTabIndex));

	Q_UNUSED(VisibleBlocks);
	return;
}

Export2BinGroupBox::Export2BinGroupBox(QWidget* parent, mlMainWindow* parent_window) : QGroupBox(parent), parentWindow(parent_window)
{
	this->setAcceptDrops(true);
}

void Export2BinGroupBox::dragEnterEvent(QDragEnterEvent* event)
{
	event->acceptProposedAction();
}

void Export2BinGroupBox::dropEvent(QDropEvent* event)
{
	const QMimeData* mimeData = event->mimeData();

	if (parentWindow == NULL)
	{
		return;
	}

	if (mimeData->hasUrls())
	{
		QStringList pathList;
		QList<QUrl> urlList = mimeData->urls();

		QDir working_dir(parentWindow->mToolsPath);
		for (int i = 0; i < urlList.size(); i++)
		{
			pathList.append(urlList.at(i).toLocalFile());
		}
		
		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));

		bool allowOverwrite = this->parentWindow->mExport2BinOverwriteWidget->isChecked();

		QString outputDir = parentWindow->mExport2BinTargetDirWidget->text();
		parentWindow->StartConvertThread(pathList, outputDir, allowOverwrite);
		
		event->acceptProposedAction();
	}
}

void Export2BinGroupBox::dragLeaveEvent(QDragLeaveEvent* event)
{
	event->accept();
}
