//===-- SectionLoadHistory.cpp ----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Target/SectionLoadHistory.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/Stream.h"
#include "lldb/Target/SectionLoadList.h"

using namespace lldb;
using namespace lldb_private;


bool
SectionLoadHistory::IsEmpty() const
{
    Mutex::Locker locker(m_mutex);
    return m_stop_id_to_section_load_list.empty();
}

void
SectionLoadHistory::Clear ()
{
    Mutex::Locker locker(m_mutex);
    m_stop_id_to_section_load_list.clear();
}

uint32_t
SectionLoadHistory::GetLastStopID() const
{
    Mutex::Locker locker(m_mutex);
    if (m_stop_id_to_section_load_list.empty())
        return 0;
    else
        return m_stop_id_to_section_load_list.rbegin()->first;
}

SectionLoadList *
SectionLoadHistory::GetSectionLoadListForStopID (uint32_t stop_id, bool read_only)
{
    if (!m_stop_id_to_section_load_list.empty())
    {
        if (read_only)
        {
            // The section load list is for reading data only so we don't need to create
            // a new SectionLoadList for the current stop ID, just return the section
            // load list for the stop ID that is equal to or less than the current stop ID
            if (stop_id == eStopIDNow)
            {
                // If we are asking for the latest and greatest value, it is always
                // at the end of our list becuase that will be the highest stop ID.
                StopIDToSectionLoadList::reverse_iterator rpos = m_stop_id_to_section_load_list.rbegin();
                return rpos->second.get();
            }
            else
            {
                StopIDToSectionLoadList::iterator pos = m_stop_id_to_section_load_list.lower_bound(stop_id);
                if (pos != m_stop_id_to_section_load_list.end() && pos->first == stop_id)
                    return pos->second.get();
                else if (pos != m_stop_id_to_section_load_list.begin())
                {
                    --pos;
                    return pos->second.get();
                }
            }
        }
        else
        {
            // You can only use "eStopIDNow" when reading from the section load history
            assert(stop_id != eStopIDNow);

            // We are updating the section load list (not read only), so if the stop ID
            // passed in isn't the same as the last stop ID in our collection, then create
            // a new node using the current stop ID
            StopIDToSectionLoadList::iterator pos = m_stop_id_to_section_load_list.lower_bound(stop_id);
            if (pos != m_stop_id_to_section_load_list.end() && pos->first == stop_id)
            {
                // We already have an entry for this value
                return pos->second.get();
            }
            
            // We must make a new section load list that is based on the last valid
            // section load list, so here we copy the last section load list and add
            // a new node for the current stop ID.
            StopIDToSectionLoadList::reverse_iterator rpos = m_stop_id_to_section_load_list.rbegin();
            SectionLoadListSP section_load_list_sp(new SectionLoadList(*rpos->second.get()));
            m_stop_id_to_section_load_list[stop_id] = section_load_list_sp;
            return section_load_list_sp.get();
        }
    }
    SectionLoadListSP section_load_list_sp(new SectionLoadList());
    if (stop_id == eStopIDNow)
        stop_id = 0;
    m_stop_id_to_section_load_list[stop_id] = section_load_list_sp;
    return section_load_list_sp.get();
}

SectionLoadList &
SectionLoadHistory::GetCurrentSectionLoadList ()
{
    const bool read_only = true;
    Mutex::Locker locker(m_mutex);
    SectionLoadList *section_load_list = GetSectionLoadListForStopID (eStopIDNow, read_only);
    assert(section_load_list != NULL);
    return *section_load_list;
}

addr_t
SectionLoadHistory::GetSectionLoadAddress (uint32_t stop_id, const lldb::SectionSP &section_sp)
{
    Mutex::Locker locker(m_mutex);
    const bool read_only = true;
    SectionLoadList *section_load_list = GetSectionLoadListForStopID (stop_id, read_only);
    return section_load_list->GetSectionLoadAddress(section_sp);
}

bool
SectionLoadHistory::ResolveLoadAddress (uint32_t stop_id, addr_t load_addr, Address &so_addr)
{
    // First find the top level section that this load address exists in
    Mutex::Locker locker(m_mutex);
    const bool read_only = true;
    SectionLoadList *section_load_list = GetSectionLoadListForStopID (stop_id, read_only);
    return section_load_list->ResolveLoadAddress (load_addr, so_addr);
}

bool
SectionLoadHistory::SetSectionLoadAddress (uint32_t stop_id,
                                           const lldb::SectionSP &section_sp,
                                           addr_t load_addr,
                                           bool warn_multiple)
{
    Mutex::Locker locker(m_mutex);
    const bool read_only = false;
    SectionLoadList *section_load_list = GetSectionLoadListForStopID (stop_id, read_only);
    return section_load_list->SetSectionLoadAddress(section_sp, load_addr, warn_multiple);
}

size_t
SectionLoadHistory::SetSectionUnloaded (uint32_t stop_id, const lldb::SectionSP &section_sp)
{
    Mutex::Locker locker(m_mutex);
    const bool read_only = false;
    SectionLoadList *section_load_list = GetSectionLoadListForStopID (stop_id, read_only);
    return section_load_list->SetSectionUnloaded (section_sp);
}

bool
SectionLoadHistory::SetSectionUnloaded (uint32_t stop_id, const lldb::SectionSP &section_sp, addr_t load_addr)
{
    Mutex::Locker locker(m_mutex);
    const bool read_only = false;
    SectionLoadList *section_load_list = GetSectionLoadListForStopID (stop_id, read_only);
    return section_load_list->SetSectionUnloaded (section_sp, load_addr);
}

void
SectionLoadHistory::Dump (Stream &s, Target *target)
{
    Mutex::Locker locker(m_mutex);
    StopIDToSectionLoadList::iterator pos, end = m_stop_id_to_section_load_list.end();
    for (pos = m_stop_id_to_section_load_list.begin(); pos != end; ++pos)
    {
        s.Printf("StopID = %u:\n", pos->first);
        pos->second->Dump(s, target);
        s.EOL();
    }
}


