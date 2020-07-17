// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAUTIL_COMPRESSION_HPP
#define XAYAUTIL_COMPRESSION_HPP

#include <json/json.h>

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

/**
 * Tries to encode the given JSON value to a compressed representation.
 * This only works for JSON objects and arrays.  Returns true on success,
 * and false otherwise.  Also the uncompressed, serialised data
 * is returned, which might be used by the caller as well (e.g. to hash it,
 * or to enforce that its length is less than the maxOutputSize that will be
 * used with UncompressJson later).
 *
 * This method is a wrapper around CompressData, which also takes care of
 * serialising the JSON value accordingly.  It furthermore base64-encodes
 * the compressed output, so that it can itself be used as string inside
 * a JSON value easily.
 */
bool CompressJson (const Json::Value& val,
                   std::string& encoded, std::string& uncompressed);

/**
 * Uncompresses an encoded JSON value (from CompressJson).  Returns true
 * if it was successful, which means that it was valid base64, could
 * be uncompressed (including the maxOutputSize check) with UncompressData,
 * and was a valid JSON array or object.
 *
 * This also returns the uncompressed but still serialised JSON string,
 * which can be useful e.g. for computing hashes of the data and comparing
 * to previous commitments (which is hard to do on the JSON itself).
 *
 * Parsing the JSON value enforces the given stack limit (for recursive
 * parsing of e.g. objects inside objects), which should be chosen
 * small enough to ensure safe and quick parsing, but large enough so that
 * it is sufficient for whatever JSON values are expected.
 */
bool UncompressJson (const std::string& input,
                     size_t maxOutputSize, unsigned stackLimit,
                     Json::Value& output, std::string& uncompressed);

} // namespace xaya

#endif // XAYAUTIL_COMPRESSION_HPP
