/*
 * Copyright (c) 2013-14, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include <assert.h>
#include <string.h>
#include "blfwk/DataSource.h"
#include "blfwk/DataTarget.h"

using namespace blfwk;

#pragma warning(disable : 4068) /* disable unknown pragma warnings */
#pragma mark *** DataSource::PatternSegment ***

DataSource::PatternSegment::PatternSegment(DataSource &source)
    : DataSource::Segment(source)
    , m_pattern()
{
}

DataSource::PatternSegment::PatternSegment(DataSource &source, const SizedIntegerValue &pattern)
    : DataSource::Segment(source)
    , m_pattern(pattern)
{
}

DataSource::PatternSegment::PatternSegment(DataSource &source, uint8_t pattern)
    : DataSource::Segment(source)
    , m_pattern(static_cast<uint8_t>(pattern))
{
}

DataSource::PatternSegment::PatternSegment(DataSource &source, uint16_t pattern)
    : DataSource::Segment(source)
    , m_pattern(static_cast<uint16_t>(pattern))
{
}

DataSource::PatternSegment::PatternSegment(DataSource &source, uint32_t pattern)
    : DataSource::Segment(source)
    , m_pattern(static_cast<uint32_t>(pattern))
{
}

unsigned DataSource::PatternSegment::getData(unsigned offset, unsigned maxBytes, uint8_t *buffer)
{
    memset(buffer, 0, maxBytes);

    return maxBytes;
}

//! The pattern segment's length is a function of the data target. If the
//! target is bounded, then the segment's length is simply the target's
//! length. Otherwise, if no target has been set or the target is unbounded,
//! then the length returned is 0.
unsigned DataSource::PatternSegment::getLength()
{
    DataTarget *target = m_source.getTarget();
    if (!target)
    {
        return 0;
    }

    uint32_t length;
    if (target->isBounded())
    {
        length = target->getEndAddress() - target->getBeginAddress();
    }
    else
    {
        length = m_pattern.getSize();
    }
    return length;
}

#pragma mark *** PatternSource ***

PatternSource::PatternSource()
    : DataSource()
    , DataSource::PatternSegment((DataSource &)*this)
{
}

PatternSource::PatternSource(const SizedIntegerValue &value)
    : DataSource()
    , DataSource::PatternSegment((DataSource &)*this, value)
{
}

#pragma mark *** UnmappedDataSource ***

UnmappedDataSource::UnmappedDataSource()
    : DataSource()
    , DataSource::Segment((DataSource &)*this)
    , m_data()
    , m_length(0)
{
}

UnmappedDataSource::UnmappedDataSource(const uint8_t *data, unsigned length)
    : DataSource()
    , DataSource::Segment((DataSource &)*this)
    , m_data()
    , m_length(0)
{
    setData(data, length);
}

//! Makes a copy of \a data that is freed when the data source is
//! destroyed. The caller does not have to maintain \a data after this call
//! returns.
void UnmappedDataSource::setData(const uint8_t *data, unsigned length)
{
    m_data.safe_delete();

    uint8_t *dataCopy = new uint8_t[length];
    memcpy(dataCopy, data, length);
    m_data = dataCopy;
    m_length = length;
}

unsigned UnmappedDataSource::getData(unsigned offset, unsigned maxBytes, uint8_t *buffer)
{
    assert(offset < m_length);
    unsigned copyBytes = std::min<unsigned>(m_length - offset, maxBytes);
    memcpy(buffer, &m_data[offset], copyBytes);
    return copyBytes;
}

//! The unmapped datasource as segment's base address is a function of the data target.
//! If the target is not NULL, then the segment's base address is simply the target's
//! beginAddress. Otherwise, if no target has been set, then the base address returned is 0.
uint32_t UnmappedDataSource::getBaseAddress()
{
    return getTarget() == NULL ? 0 : getTarget()->getBeginAddress();
}

#pragma mark *** MemoryImageDataSource ***

MemoryImageDataSource::MemoryImageDataSource(StExecutableImage *image)
    : DataSource()
    , m_image(image)
{
    // reserve enough room for all segments
    m_segments.reserve(m_image->getRegionCount());
}

MemoryImageDataSource::~MemoryImageDataSource()
{
    segment_array_t::iterator it = m_segments.begin();
    for (; it != m_segments.end(); ++it)
    {
        // delete this segment if it has been created
        if (*it)
        {
            delete *it;
        }
    }
}

unsigned MemoryImageDataSource::getSegmentCount()
{
    return m_image->getRegionCount();
}

DataSource::Segment *MemoryImageDataSource::getSegmentAt(unsigned index)
{
    // return previously created segment
    if (index < m_segments.size() && m_segments[index])
    {
        return m_segments[index];
    }

    // extend array out to this index
    if (index >= m_segments.size() && index < m_image->getRegionCount())
    {
        m_segments.resize(index + 1, NULL);
    }

    // create the new segment object
    DataSource::Segment *newSegment;
    const StExecutableImage::MemoryRegion &region = m_image->getRegionAtIndex(index);
    if (region.m_type == StExecutableImage::TEXT_REGION)
    {
        newSegment = new TextSegment(*this, m_image, index);
    }
    else if (region.m_type == StExecutableImage::FILL_REGION)
    {
        newSegment = new FillSegment(*this, m_image, index);
    }
    else
    {
        newSegment = NULL;
    }

    m_segments[index] = newSegment;
    return newSegment;
}

#pragma mark *** MemoryImageDataSource::TextSegment ***

MemoryImageDataSource::TextSegment::TextSegment(MemoryImageDataSource &source, StExecutableImage *image, unsigned index)
    : DataSource::Segment(source)
    , m_image(image)
    , m_index(index)
{
}

unsigned MemoryImageDataSource::TextSegment::getData(unsigned offset, unsigned maxBytes, uint8_t *buffer)
{
    const StExecutableImage::MemoryRegion &region = m_image->getRegionAtIndex(m_index);
    assert(region.m_type == StExecutableImage::TEXT_REGION);

    unsigned copyBytes = std::min<unsigned>(region.m_length - offset, maxBytes);
    memcpy(buffer, &region.m_data[offset], copyBytes);

    return copyBytes;
}

unsigned MemoryImageDataSource::TextSegment::getLength()
{
    const StExecutableImage::MemoryRegion &region = m_image->getRegionAtIndex(m_index);
    return region.m_length;
}

uint32_t MemoryImageDataSource::TextSegment::getBaseAddress()
{
    const StExecutableImage::MemoryRegion &region = m_image->getRegionAtIndex(m_index);
    return region.m_address;
}

#pragma mark *** MemoryImageDataSource::FillSegment ***

MemoryImageDataSource::FillSegment::FillSegment(MemoryImageDataSource &source, StExecutableImage *image, unsigned index)
    : DataSource::PatternSegment(source)
    , m_image(image)
    , m_index(index)
{
    SizedIntegerValue zero(0, kWordSize);
    setPattern(zero);
}

unsigned MemoryImageDataSource::FillSegment::getLength()
{
    const StExecutableImage::MemoryRegion &region = m_image->getRegionAtIndex(m_index);
    return region.m_length;
}

uint32_t MemoryImageDataSource::FillSegment::getBaseAddress()
{
    const StExecutableImage::MemoryRegion &region = m_image->getRegionAtIndex(m_index);
    return region.m_address;
}
