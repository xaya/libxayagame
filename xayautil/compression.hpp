// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAUTIL_COMPRESSION_HPP
#define XAYAUTIL_COMPRESSION_HPP

#include <cstddef>
#include <string>

namespace xaya
{

/**
 * Tries to compress the given byte-string, returning the output bytes.
 * This uses a specific compression format (raw deflate with windowBits set
 * to 15), which is guaranteed to be accepted by UncompressData.
 *
 * Note that compression should be used only selectively in games, i.e. in
 * situations where e.g. compressing the move data makes a clear difference
 * for instance to fit inside the 2k value limit.  It should not be used
 * "just to optimise" the data size, since general compression of transaction
 * data is best handled by Xaya Core itself rather than individual games
 * (and can then be done in a non-consensus-relevant way, which is more
 * robust and safer).
 */
std::string CompressData (const std::string& data);

/**
 * Tries to uncompress the given byte-string, returning the original data.
 * If the input data is invalid or the output size is larger than maxOutputSize,
 * this function returns false instead (indicating an error).
 *
 * By setting a specific, explicit maxOutputSize, we ensure that maliciously
 * crafted data cannot be used to DoS a node on memory (i.e. "zip bomb").
 * Thus users should ensure they pass a reasonable and not insanely large
 * value for this parameter, as fits to the application in question.  The value
 * used here is then relevant for consensus!
 *
 * This tries to decompress the data as raw deflate stream with windowBits
 * set to 15.  It is guaranteed to stay stable (in particular also with
 * respect to what data exactly it accepts as valid), so that consensus can
 * rely on success or failure of the function.
 */
bool UncompressData (const std::string& input, size_t maxOutputSize,
                     std::string& output);

} // namespace xaya

#endif // XAYAUTIL_COMPRESSION_HPP
