// Copyright (c) Charles J. Cliffe
// SPDX-License-Identifier: GPL-2.0+

#include "BookmarkMgr.h"
#include "CubicSDR.h"
#include "DataTree.h"
#include <wx/string.h>

#define BOOKMARK_RECENTS_MAX 25

BookmarkEntry::~BookmarkEntry() {
    delete node;
}

BookmarkMgr::BookmarkMgr() {
    rangesSorted = false;
}
//
void BookmarkMgr::saveToFile(std::string bookmarkFn, bool backup, bool useFullpath) {

	std::lock_guard < std::recursive_mutex > lock(busy_lock);

    DataTree s("cubicsdr_bookmarks");
    DataNode *header = s.rootNode()->newChild("header");
    header->newChild("version")->element()->set(wxString(CUBICSDR_VERSION).ToStdWstring());

    DataNode *branches = s.rootNode()->newChild("branches");
    
    *branches->newChild("active") = wxGetApp().getAppFrame()->getBookmarkView()->getExpandState("active")?1:0;
    *branches->newChild("range") = wxGetApp().getAppFrame()->getBookmarkView()->getExpandState("range")?1:0;
    *branches->newChild("bookmark") = wxGetApp().getAppFrame()->getBookmarkView()->getExpandState("bookmark")?1:0;
    *branches->newChild("recent") = wxGetApp().getAppFrame()->getBookmarkView()->getExpandState("recent")?1:0;

    DataNode *view_ranges = s.rootNode()->newChild("ranges");

    for (auto re_i : ranges) {
        DataNode *range = view_ranges->newChild("range");
        *range->newChild("label") = re_i->label;
        *range->newChild("freq") = re_i->freq;
        *range->newChild("start") = re_i->startFreq;
        *range->newChild("end") = re_i->endFreq;
    }

    DataNode *modems = s.rootNode()->newChild("modems");
    
    for (auto &bmd_i : bmData) {
        DataNode *group = modems->newChild("group");
        *group->newChild("@name") = bmd_i.first;
        *group->newChild("@expanded") = (getExpandState(bmd_i.first)?std::string("true"):std::string("false"));

        for (auto &bm_i : bmd_i.second ) {

			//if a matching demodulator exists, use its data instead to be be saved, because output_device could have been
			//modified by the user. So, save that "live" version instead.
			auto matchingDemod = wxGetApp().getDemodMgr().getLastDemodulatorWith(bm_i->type,
				bm_i->label,
				bm_i->frequency,
				bm_i->bandwidth);

			if (matchingDemod != nullptr) {

				wxGetApp().getDemodMgr().saveInstance(group->newChild("modem"), matchingDemod);
			}
			else {
				group->newChildCloneFrom("modem", bm_i->node);
			}
        }
    }

    DataNode *recent_modems = s.rootNode()->newChild("recent_modems");
    
    for (auto demod : wxGetApp().getDemodMgr().getDemodulators()) {
        wxGetApp().getDemodMgr().saveInstance(recent_modems->newChild("modem"),demod);
    }

    for (auto &r_i : this->recents) {
        recent_modems->newChildCloneFrom("modem", r_i->node);
    }

	wxFileName saveFile;
	wxFileName saveFileBackup;

	if (useFullpath) {
		saveFile.Assign(bookmarkFn);
		saveFileBackup.Assign(bookmarkFn + ".backup");
	}
	else {
		saveFile.Assign(wxGetApp().getConfig()->getConfigDir(), bookmarkFn);
		saveFileBackup.Assign(wxGetApp().getConfig()->getConfigDir(), bookmarkFn + ".backup");
	}

    if (saveFile.IsDirWritable()) {
        // Hopefully leave at least a readable backup in case of failure..
        if (backup && saveFile.FileExists() && (!saveFileBackup.FileExists() || saveFileBackup.IsFileWritable())) {
            wxCopyFile(saveFile.GetFullPath(wxPATH_NATIVE).ToStdString(), saveFileBackup.GetFullPath(wxPATH_NATIVE).ToStdString());
        }
        s.SaveToFileXML(saveFile.GetFullPath(wxPATH_NATIVE).ToStdString());
    }
}

bool BookmarkMgr::loadFromFile(std::string bookmarkFn, bool backup, bool useFullpath) {
	
	std::lock_guard < std::recursive_mutex > lock(busy_lock);

	wxFileName loadFile;
	wxFileName failFile;
	wxFileName lastLoaded;
	wxFileName backupFile;

	if (useFullpath) {
		loadFile.Assign(bookmarkFn);
		failFile.Assign(bookmarkFn + ".failedload");
		lastLoaded.Assign(bookmarkFn + ".lastloaded");
		backupFile.Assign(bookmarkFn + ".backup");	
	}
	else {
		loadFile.Assign(wxGetApp().getConfig()->getConfigDir(), bookmarkFn);
		failFile.Assign(wxGetApp().getConfig()->getConfigDir(), bookmarkFn + ".failedload");
		lastLoaded.Assign(wxGetApp().getConfig()->getConfigDir(), bookmarkFn + ".lastloaded");
		backupFile.Assign(wxGetApp().getConfig()->getConfigDir(), bookmarkFn + ".backup");
	}

    DataTree s;
    bool loadStatusOk = true;
    
    // File exists but is not readable
    if (loadFile.FileExists() && !loadFile.IsFileReadable()) {
        return false;
    }

    // New instance of bookmark savefiles
    if (backup && !loadFile.FileExists() && !lastLoaded.FileExists() && !backupFile.FileExists()) {
        loadDefaultRanges();
        return true;
    }
    
    // Attempt to load file
    if (!s.LoadFromFileXML(loadFile.GetFullPath(wxPATH_NATIVE).ToStdString())) {
        return false;
    }

	//Check if it is a bookmark file, read the root node.
	if (s.rootNode()->getName() != "cubicsdr_bookmarks") {
		return false;
	}
	
	// Clear any active data
	bmData.clear();
	recents.clear();
	ranges.clear();
	bmDataSorted.clear();
    
    if (s.rootNode()->hasAnother("branches")) {
        DataNode *branches = s.rootNode()->getNext("branches");
        int bActive = 1, bRange = 0, bBookmark = 1, bRecent = 1;
        if (branches->hasAnother("active")) branches->getNext("active")->element()->get(bActive);
        if (branches->hasAnother("range")) branches->getNext("range")->element()->get(bRange);
        if (branches->hasAnother("bookmark")) branches->getNext("bookmark")->element()->get(bBookmark);
        if (branches->hasAnother("recent")) branches->getNext("recent")->element()->get(bRecent);
        wxGetApp().getAppFrame()->getBookmarkView()->setExpandState("active", bActive?true:false);
        wxGetApp().getAppFrame()->getBookmarkView()->setExpandState("range", bRange?true:false);
        wxGetApp().getAppFrame()->getBookmarkView()->setExpandState("bookmark", bBookmark?true:false);
        wxGetApp().getAppFrame()->getBookmarkView()->setExpandState("recent", bRecent?true:false);
    }
    
    if (s.rootNode()->hasAnother("ranges")) {
        DataNode *view_ranges = s.rootNode()->getNext("ranges");
        while (view_ranges->hasAnother("range")) {
            DataNode *range = view_ranges->getNext("range");
            
            BookmarkRangeEntryPtr re(new BookmarkRangeEntry);
            
            if (range->hasAnother("label")) range->getNext("label")->element()->get(re->label);
            if (range->hasAnother("freq")) range->getNext("freq")->element()->get(re->freq);
            if (range->hasAnother("start")) range->getNext("start")->element()->get(re->startFreq);
            if (range->hasAnother("end")) range->getNext("end")->element()->get(re->endFreq);
            
            addRange(re);
        }
    }
 
    if (s.rootNode()->hasAnother("modems")) {
        DataNode *modems = s.rootNode()->getNext("modems");
        while (modems->hasAnother("group")) {
            DataNode *group = modems->getNext("group");
            std::string expandState = "true";
            std::string groupName = "Unnamed";
            if (group->hasAnother("@name")) {
                groupName = group->getNext("@name")->element()->toString();
            }
            if (group->hasAnother("@expanded")) {
                expandState = group->getNext("@expanded")->element()->toString();
            }
            setExpandState(groupName, (expandState == "true"));
            while (group->hasAnother("modem")) {
                DataNode *modem = group->getNext("modem");
                BookmarkEntryPtr be = nodeToBookmark(modem);
                if (be) {
                    addBookmark(groupName.c_str(), be);
                } else {
                    std::cout << "error loading bookmarked modem.." << std::endl;
                    loadStatusOk = false;
                }
            }
        }
    }
    
    if (s.rootNode()->hasAnother("recent_modems")) {
        DataNode *recent_modems = s.rootNode()->getNext("recent_modems");
        
        while (recent_modems->hasAnother("modem")) {
            DataNode *modem = recent_modems->getNext("modem");
            BookmarkEntryPtr be = nodeToBookmark(modem);
            if (be) {
                addRecent(be);
            } else {
                std::cout << "error loading recent modem.." << std::endl;
                loadStatusOk = false;
            }
        }
    }

    if (backup) {
        if (loadStatusOk) {  // Loaded OK; keep a copy
            if (loadFile.IsDirWritable()) {
                if (loadFile.FileExists() && (!lastLoaded.FileExists() || lastLoaded.IsFileWritable())) {
                    wxCopyFile(loadFile.GetFullPath(wxPATH_NATIVE).ToStdString(), lastLoaded.GetFullPath(wxPATH_NATIVE).ToStdString());
                }
            }
        } else if (!loadStatusOk) {
            if (loadFile.IsDirWritable()) { // Load failed; keep a copy of the failed bookmark file for analysis?
                if (loadFile.FileExists() && (!failFile.FileExists() || failFile.IsFileWritable())) {
                    wxCopyFile(loadFile.GetFullPath(wxPATH_NATIVE).ToStdString(), failFile.GetFullPath(wxPATH_NATIVE).ToStdString());
                }
            }
        }
    }
    
    return loadStatusOk;
}

void BookmarkMgr::loadDefaultRanges() {

    addRange(std::make_shared<BookmarkRangeEntry>(L"2200 Meters (135.7-137.8 kHz)", 136750, 135700, 137800));
    addRange(std::make_shared<BookmarkRangeEntry>(L"630 Meters (472-479 kHz)", 475500, 472000, 479000));
	addRange(std::make_shared<BookmarkRangeEntry>(L"160 Meters (1.8-2 MHz)", 1900000, 1800000, 2000000));
	addRange(std::make_shared<BookmarkRangeEntry>(L"80 Meters (3.5-4.0 MHz)", 3750000, 3500000, 4000000));
	addRange(std::make_shared<BookmarkRangeEntry>(L"60 Meters (5.332-5.405Mhz)", 5368500, 5332000, 5405000));
	addRange(std::make_shared<BookmarkRangeEntry>(L"40 Meters (7.0-7.3 MHz)", 7150000, 7000000, 7300000));
	addRange(std::make_shared<BookmarkRangeEntry>(L"30 Meters (10.1-10.15 MHz)", 10125000, 10100000, 10150000));
	addRange(std::make_shared<BookmarkRangeEntry>(L"20 Meters (14.0-14.35 MHz)", 14175000, 14000000, 14350000));
	addRange(std::make_shared<BookmarkRangeEntry>(L"17 Meters (17.044-19.092 MHz)", 18068180, 17044180, 19092180));
	addRange(std::make_shared<BookmarkRangeEntry>(L"15 Meters (21-21.45 MHz)", 21225000, 21000000, 21450000));
	addRange(std::make_shared<BookmarkRangeEntry>(L"12 Meters (24.89-24.99 MHz)", 24940000, 24890000, 24990000));
	addRange(std::make_shared<BookmarkRangeEntry>(L"10 Meters (28-29.7 MHz)", 28850000, 28000000, 29700000));
    addRange(std::make_shared<BookmarkRangeEntry>(L"6 Meters (50-54 MHz)", 52000000, 50000000, 54000000));
    addRange(std::make_shared<BookmarkRangeEntry>(L"4 Meters (70-70.5 MHz)", 70250000, 70000000, 70500000));
    addRange(std::make_shared<BookmarkRangeEntry>(L"2 Meters (144-148 MHz)", 146000000, 144000000, 148000000));
    addRange(std::make_shared<BookmarkRangeEntry>(L"1.25 Meters (219-225 MHz)", 222000000, 219000000, 225000000));
    addRange(std::make_shared<BookmarkRangeEntry>(L"70 cm (420-450 MHz)", 435000000, 420000000, 450000000));
    addRange(std::make_shared<BookmarkRangeEntry>(L"33 cm (902-928 MHz)", 915000000, 902000000, 928000000));
    addRange(std::make_shared<BookmarkRangeEntry>(L"23 cm (1240-1300 MHz)", 1270000000, 1240000000, 1300000000));
    addRange(std::make_shared<BookmarkRangeEntry>(L"13 cm lower (2300-2310 MHz)", 2305000000, 2300000000, 2310000000));
    addRange(std::make_shared<BookmarkRangeEntry>(L"13 cm upper (2390-2450 MHz)", 2420000000, 2390000000, 2450000000));
}

void BookmarkMgr::resetBookmarks() {

	std::lock_guard < std::recursive_mutex > lock(busy_lock);

	// Clear any active data
	bmData.clear();
	recents.clear();
	ranges.clear();
	bmDataSorted.clear();

	loadDefaultRanges();

}

bool BookmarkMgr::hasLastLoad(std::string bookmarkFn) {
    wxFileName lastLoaded(wxGetApp().getConfig()->getConfigDir(), bookmarkFn + ".lastloaded");
    return lastLoaded.FileExists() && lastLoaded.IsFileReadable();
}

bool BookmarkMgr::hasBackup(std::string bookmarkFn) {
    wxFileName backupFile(wxGetApp().getConfig()->getConfigDir(), bookmarkFn + ".backup");
    return backupFile.FileExists() && backupFile.IsFileReadable();
}

void BookmarkMgr::addBookmark(std::string group, DemodulatorInstancePtr demod) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
	//Create a BookmarkEntry from demod data, saving its
	//characteristics in be->node.
    BookmarkEntryPtr be = demodToBookmarkEntry(demod);
     
    bmData[group].push_back(be);
    bmDataSorted[group] = false;
}

void BookmarkMgr::addBookmark(std::string group, BookmarkEntryPtr be) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    bmData[group].push_back(be);
    bmDataSorted[group] = false;
}


void BookmarkMgr::removeBookmark(std::string group, BookmarkEntryPtr be) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
   
    if (bmData.find(group) == bmData.end()) {
        return;
    }

    BookmarkList::iterator i = std::find(bmData[group].begin(), bmData[group].end(), be);
    
    if (i != bmData[group].end()) {

        bmData[group].erase(i);
    }
}

void BookmarkMgr::removeBookmark(BookmarkEntryPtr be) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    for (auto &bmd_i : bmData) {
        BookmarkList::iterator i = std::find(bmd_i.second.begin(), bmd_i.second.end(), be);
        if (i != bmd_i.second.end()) {
            bmd_i.second.erase(i);
        }
    }
}

void BookmarkMgr::moveBookmark(BookmarkEntryPtr be, std::string group) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    for (auto &bmd_i : bmData) {
        BookmarkList::iterator i = std::find(bmd_i.second.begin(), bmd_i.second.end(), be);
        if (i != bmd_i.second.end()) {
            if (bmd_i.first == group) {
                return;
            }
            bmData[group].push_back(*i);
            bmd_i.second.erase(i);
            bmDataSorted[group] = false;
            bmDataSorted[bmd_i.first] = false;
            return;
        }
    }
}


void BookmarkMgr::addGroup(std::string group) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    if (bmData.find(group) == bmData.end()) {
        BookmarkList dummy = bmData[group];
    }
}

void BookmarkMgr::removeGroup(std::string group) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    BookmarkMap::iterator i = bmData.find(group);
    
    if (i != bmData.end()) {
       
        bmData.erase(group);
    }
}

void BookmarkMgr::renameGroup(std::string group, std::string ngroup) {
    if (group == ngroup) {
        return;
    }
    
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    BookmarkMap::iterator i = bmData.find(group);
    BookmarkMap::iterator it = bmData.find(ngroup);
    
    if (i != bmData.end() && it != bmData.end()) {
        for (auto ii : bmData[group]) {
            bmData[ngroup].push_back(ii);
        }
        bmData.erase(group);
    } else if (i != bmData.end()) {
        bmData[ngroup] = bmData[group];
        bmData.erase(group);
    }
}

BookmarkList BookmarkMgr::getBookmarks(std::string group) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    if (bmData.find(group) == bmData.end()) {
        return BookmarkList();
    }
    
    if (!bmDataSorted[group]) {
		std::sort(bmData[group].begin(), bmData[group].end(), [](const BookmarkEntryPtr a, const BookmarkEntryPtr b) -> bool { return a->frequency < b->frequency;});
        bmDataSorted[group] = true;
    }
    
    return bmData[group];
}


void BookmarkMgr::getGroups(BookmarkNames &arr) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);

    for (BookmarkMap::iterator i = bmData.begin(); i!= bmData.end(); ++i) {
        arr.push_back(i->first);
    }
}

void BookmarkMgr::getGroups(wxArrayString &arr) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);

    for (BookmarkMap::iterator i = bmData.begin(); i!= bmData.end(); ++i) {
        arr.push_back(i->first);
    }
}


void BookmarkMgr::setExpandState(std::string groupName, bool state) {
	std::lock_guard < std::recursive_mutex > lock(busy_lock);

    expandState[groupName] = state;
}


bool BookmarkMgr::getExpandState(std::string groupName) {
	
	std::lock_guard < std::recursive_mutex > lock(busy_lock);

    if (expandState.find(groupName) == expandState.end()) {
        return true;
    }
    return expandState[groupName];
}


void BookmarkMgr::updateActiveList() {

    if (wxGetApp().isShuttingDown()) {
        return;
    }
	
	BookmarkView *bmv = wxGetApp().getAppFrame()->getBookmarkView();
    
    if (bmv) {
        bmv->updateActiveList();
    }
}

void BookmarkMgr::updateBookmarks() {

    BookmarkView *bmv = wxGetApp().getAppFrame()->getBookmarkView();
    
    if (bmv) {
        bmv->updateBookmarks();
    }
}

void BookmarkMgr::updateBookmarks(std::string group) {

    BookmarkView *bmv = wxGetApp().getAppFrame()->getBookmarkView();
    
    if (bmv) {
        bmv->updateBookmarks(group);
    }
}


void BookmarkMgr::addRecent(DemodulatorInstancePtr demod) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);

    recents.push_back(demodToBookmarkEntry(demod));

    trimRecents();
}

void BookmarkMgr::addRecent(BookmarkEntryPtr be) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);

    recents.push_back(be);

    trimRecents();
}



void BookmarkMgr::removeRecent(BookmarkEntryPtr be) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    BookmarkList::iterator bm_i = std::find(recents.begin(),recents.end(), be);
 
    if (bm_i != recents.end()) {
        recents.erase(bm_i);
    }
}


BookmarkList BookmarkMgr::getRecents() {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    //return a copy 
	return recents;
}


void BookmarkMgr::clearRecents() {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);

    recents.clear();
}


void BookmarkMgr::trimRecents() {
    if (recents.size() > BOOKMARK_RECENTS_MAX) {
       
        recents.erase(recents.begin(), recents.begin()+1);
    }
}


void BookmarkMgr::addRange(BookmarkRangeEntryPtr re) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    ranges.push_back(re);
    rangesSorted = false;
}

void BookmarkMgr::removeRange(BookmarkRangeEntryPtr re) {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);
    
    BookmarkRangeList::iterator re_i = std::find(ranges.begin(), ranges.end(), re);
    
    if (re_i != ranges.end()) {

        ranges.erase(re_i);
    }
}


BookmarkRangeList BookmarkMgr::getRanges() {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);

    if (!rangesSorted) {
        std::sort(ranges.begin(), ranges.end(), [](const BookmarkRangeEntryPtr a, const BookmarkRangeEntryPtr b) -> bool { return a->freq < b->freq;});
        rangesSorted = true;
    }
    
    return ranges;
}


void BookmarkMgr::clearRanges() {
    std::lock_guard < std::recursive_mutex > lock(busy_lock);

    ranges.clear();
}


BookmarkEntryPtr BookmarkMgr::demodToBookmarkEntry(DemodulatorInstancePtr demod) {
    
    BookmarkEntryPtr be(new BookmarkEntry);
    
    be->bandwidth = demod->getBandwidth();
    be->type = demod->getDemodulatorType();
    be->label = demod->getDemodulatorUserLabel();
    be->frequency = demod->getFrequency();

    //fine to do so here, so long nobody overrides be->node, DataNode will be 
    //deleted at last BookmarkEntryPtr be ref.
    be->node = new DataNode;
    wxGetApp().getDemodMgr().saveInstance(be->node, demod);
    
    return be;
}

std::wstring BookmarkMgr::getSafeWstringValue(DataNode* node, const std::string& childNodeName) {
    
    std::wstring decodedWString = L"";

    if (node != nullptr) {

        DataNode* childNode = node->getNext(childNodeName.c_str());

        //1) decode as encoded wstring:
        try {
            childNode->element()->get(decodedWString);

        } catch (DataTypeMismatchException e) {
            //2) wstring decode fail, try simple std::string
            std::string decodedStdString;
            try {
                
                childNode->element()->get(decodedStdString);

                //use wxString for a clean conversion to a wstring:
                decodedWString = wxString(decodedStdString).ToStdWstring();

            } catch (DataTypeMismatchException e) {
                //nothing works, return an empty string.
                decodedWString = L"";
            }
        }
    }

    return decodedWString;
}

BookmarkEntryPtr BookmarkMgr::nodeToBookmark(DataNode *node) {
    if (!node->hasAnother("frequency") || !node->hasAnother("type") || !node->hasAnother("bandwidth")) {
        return nullptr;
    }
    
    BookmarkEntryPtr be(new BookmarkEntry());

    node->getNext("frequency")->element()->get(be->frequency);
    node->getNext("type")->element()->get(be->type);
    node->getNext("bandwidth")->element()->get(be->bandwidth);

    if (node->hasAnother("user_label")) {
        be->label = BookmarkMgr::getSafeWstringValue( node, "user_label");
    }

    node->rewindAll();

    //fine to do so here, so long nobody overrides be->node, DataNode will be 
    //deleted at last BookmarkEntryPtr be ref.
    //copy data from *node.
    be->node = new DataNode("node",*node);
    
    return be;
}


std::wstring BookmarkMgr::getBookmarkEntryDisplayName(BookmarkEntryPtr bmEnt) {
    std::wstring dispName = bmEnt->label;
    
    if (dispName == L"") {
        std::string freqStr = frequencyToStr(bmEnt->frequency) + " " + bmEnt->type;

        dispName = wxString(freqStr).ToStdWstring();
    }
    
    return dispName;
}

std::wstring BookmarkMgr::getActiveDisplayName(DemodulatorInstancePtr demod) {
    std::wstring activeName = demod->getDemodulatorUserLabel();
    
    if (activeName == L"") {
        std::string wstr = frequencyToStr(demod->getFrequency()) + " " + demod->getDemodulatorType();

        activeName = wxString(wstr).ToStdWstring();
    }
    
    return activeName;
}

void BookmarkMgr::removeActive(DemodulatorInstancePtr demod) {
	
	if (demod == nullptr) {
		return;
	}

	//Delete demodulator
	wxGetApp().getDemodMgr().setActiveDemodulator(nullptr, true);
	wxGetApp().getDemodMgr().setActiveDemodulator(nullptr, false);
	wxGetApp().removeDemodulator(demod);
	wxGetApp().getDemodMgr().deleteThread(demod);
}

