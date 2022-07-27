#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <thread>
#include <future>
#include <unordered_map>
#include <iostream>
#include "HexCore.h"

namespace fs = std::filesystem;

std::size_t RawBytesHasher::operator()(const RawBytes& hex) const
{
	std::size_t seed = hex.get().size();
	for (int i = 0; i < hex.get().size(); ++i)
	{
		seed ^= hex.get().at(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
	return seed;
}


std::optional<RawBytes> RawBytes::make_hex(std::string_view str) noexcept
{
	try
	{
		return RawBytes{ str };
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what();
	}
	catch (...)
	{
		std::cerr << "Unexpected error, nullopt returned";
	}

	return {};
}
bool RawBytes::operator==(const RawBytes& other) const noexcept
{
	return this->get() == other.get();
}
bool RawBytes::operator<(const RawBytes& other) const noexcept
{
	return this->get() < other.get();
}
inline const std::vector<char>& RawBytes::get() const noexcept
{
	return this->seq;
}
inline size_t RawBytes::size() const noexcept
{
	return this->get().size();
}
RawBytes::RawBytes(std::string_view src) : seq(src.size())
{
	for (size_t i = 0; i < src.size(); ++i)
		is_hex(src.at(i)) ? this->seq.at(i) = to_hex(src.at(i)) : throw std::invalid_argument(src.at(i) + " is not hex");
	shrink_and_resize();
}
RawBytesIt RawBytes::shrink_bits_base(RawBytesIt first, RawBytesIt last)
{
	if (first > last)
		throw std::logic_error("first iter > last iter");

	RawBytesIt save_pos = first;
	while (first != last)
	{
		if (first + 1 != last)
		{
			*save_pos = *first << 4 | *(first + 1);
			first += 2;
		}
		else
		{
			*save_pos = *first << 4;
			++first;
		}
		++save_pos;
	}

	return save_pos;
}
bool RawBytes::is_hex(char ascii) noexcept
{
	return isalnum(ascii) && to_hex(ascii) < 16;
}
char RawBytes::to_hex(char ascii) noexcept
{
	return isdigit(ascii) ? ascii - '0' : tolower(ascii) - 'a' + 10;
}
void RawBytes::shrink_and_resize()
{
	seq.resize(shrink_bits_base(seq.begin(), seq.end()) - seq.begin());
}
std::ostream& operator<<(std::ostream& os, const RawBytes& hex)
{
	for (auto elem : hex.get())
		os << std::hex << +static_cast<unsigned char>(elem);

	return os;
}
std::wostream& operator<<(std::wostream& os, const RawBytes& hex)
{
	for (auto elem : hex.get())
		os << std::hex << +static_cast<unsigned char>(elem);

	return os;
}


IfstreamSlicer::IfstreamSlicer(const Path& path, uintmax_t slice_size) : file_size{ fs::file_size(fs::path{ path }) }
{
	if (!fs::exists(path) || !fs::is_regular_file(path))
		throw std::logic_error("Invalid path");
	if (slice_size == 0)
		throw std::logic_error("Slice is empty");
	if (!file)
		throw std::runtime_error("Bad file access");
	if (slice_size == 1 || this->file_size == 0)
		this->noslice = true;

	if (this->file_size != 0)
	{
		this->file = std::ifstream{ path, std::ios::binary };
		file.unsetf(std::ios_base::skipws);
		if (!this->noslice)
		{
			slice_size = (this->file_size < slice_size) ? this->file_size : slice_size;
			this->slice_head.resize(slice_size);
			file.read(this->slice_head.data(), slice_size);
			if (file_size < slice_size * 2)
				slice_size = file_size - slice_size;
			this->slice_tail.resize(slice_size);
			file.read(this->slice_tail.data(), slice_size);
		}
	}
}
bool IfstreamSlicer::good() const noexcept
{
	return this->file_cursor < this->file_size;
}
uintmax_t IfstreamSlicer::size() const
{
	return this->file_size;
}
uintmax_t IfstreamSlicer::tellg() const
{
	return this->file_cursor;
}
std::optional<char> IfstreamSlicer::get() noexcept
{
	if (!this->file.good())
		return {};
	try
	{
		this->file_cursor += 1;
		if (this->noslice)
			return this->file.get();
		if (this->slice_cursor < this->slice_head.size())
			return this->slice_head.at(this->slice_cursor++);
		if (this->slice_cursor >= this->slice_head.size() && !this->end())
		{
			auto result = this->slice_tail.at(this->slice_cursor - this->slice_head.size());
			++this->slice_cursor;
			return result;
		}
		if (this->end() && this->file.good())
		{
			this->file_cursor -= 1;
			this->slice_head = std::move(this->slice_tail);
			auto slice_size = this->file_size - file_cursor;
			slice_size = slice_size < slice_head.size() ? slice_size : slice_head.size();
			this->slice_tail.resize(slice_size);
			file.read(this->slice_tail.data(), slice_size);
			this->slice_tail.shrink_to_fit();
			this->file_cursor += 1;
			this->slice_cursor = this->slice_head.size() + 1;
			return this->slice_tail.at(this->slice_cursor - 1 - this->slice_head.size());
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what();
	}
	catch (...)
	{
		std::cerr << "Unexpected error";
	}

	return {};
}
void IfstreamSlicer::seekg_prev(uintmax_t pos)
{
	if (pos > this->file_cursor || (!this->noslice && (this->file_cursor - pos) > slice_cursor))
		throw std::logic_error{ "Wrong streampos" };

	if (this->noslice)
		this->file.seekg(pos, std::ios_base::beg);
	else
	{
		slice_cursor -= this->file_cursor - pos;
		this->file_cursor = pos;
	}
}
bool IfstreamSlicer::end() const
{
	return this->slice_cursor == slice_head.size() + slice_tail.size();
}


SearchRes::SearchRes(RawBytesSet h, std::unordered_map<Path, std::vector<PositionsInFile>> umap, UnopenedFiles skipped) {

	for (const auto& p : umap)
		if (p.second.size() != h.size())
			throw std::logic_error("Wrong data");

	this->tofind = std::move(h);
	this->data = umap;
	this->skipped = skipped;
}
SearchRes::SearchRes(RawBytesSet h, UnopenedFiles skipped) {

	if(!skipped)
		throw std::logic_error("Wrong data");

	this->tofind = std::move(h);
	this->skipped = skipped;
}
bool SearchRes::contains(const Path& p) const
{
	return this->data.contains(p);
}
RawBytesSet::const_iterator SearchRes::rowbytes_cbegin() const
{
	return this->tofind.cbegin();
}
inline bool SearchRes::empty() const noexcept
{
	return this->data.empty();
}
RawBytesSet::const_iterator SearchRes::rowbytes_cend() const
{
	return this->tofind.cend();
}
const PositionsInFile& SearchRes::at(const Path& p, const RawBytes& h) const
{
	auto p_it = this->data.find(p);
	if (p_it == this->data.cend())
		throw std::out_of_range("No such path found");
	auto h_it = this->tofind.find(h);
	if (h_it == this->tofind.cend())
		throw std::logic_error("No such sequence found");

	return p_it->second.at(std::distance(this->tofind.cbegin(), h_it));
}
std::vector<Path> SearchRes::collect_paths() const noexcept
{
	std::vector<Path> paths;
	paths.reserve(this->data.size());
	for (const auto& e : this->data)
		paths.push_back(e.first);

	return paths;
}
const UnopenedFiles& SearchRes::unopened_files() const noexcept
{
	return this->skipped;
}
void SearchRes::reset() noexcept
{
	this->data.clear();
	this->tofind.clear();
	if(this->skipped)
		this->skipped->clear();
}

Search::Search(Path path, RawBytesSet tofind)
{
	if (!fs::exists(path))
		throw std::runtime_error("No such file or directory");

	if (fs::is_regular_file(path))
		this->paths.push_back(path);
	else
	{
		for (auto& p : fs::recursive_directory_iterator(path))
			if (!fs::is_directory(status(p)) && fs::is_regular_file(status(p)))
				this->paths.push_back(fs::path(p).wstring());
	}
}
bool Search::add_bytes(RawBytes hex) noexcept
{
	if (hex.get().empty()) return false;
	try
	{
		this->tofind.insert(hex);
	}
	catch (...)
	{
		std::cerr << "Unexpected error";
		return false;
	}

	return true;
}
bool Search::add_path(Path path) noexcept
{
	try
	{
		if (fs::is_directory(path))
		{
			for (auto& p : fs::recursive_directory_iterator(path))
				if (!fs::is_directory(status(p)) && fs::is_regular_file(status(p)))
					this->paths.push_back(fs::path(p).wstring());
		}
		else if (fs::is_regular_file(path))
		{
			this->paths.push_back(path);
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what();
		return false;
	}
	catch (...)
	{
		std::cerr << "Unexpected error";
		return false;
	}

	return true;
}
void Search::reset() noexcept
{
	this->paths.clear();
	this->tofind.clear();
	this->threads_number = std::thread::hardware_concurrency() - 1;
}
size_t Search::size() const noexcept
{
	return this->paths.size();
}
bool Search::ready() const noexcept
{
	return !this->paths.empty() && !this->tofind.empty();
}
SearchRes Search::exec_and_reset(size_t slice_size, std::atomic<unsigned>& progress)
{
	if (!this->ready()) return {};

	auto check = this->tofind.cbegin()->get();

	std::vector<std::vector<unsigned>> paths_indexes = this->group_paths_for_threads();
	std::vector<Path> unopened_files{};
	std::unordered_map<Path, std::vector<PositionsInFile>> data{};
	if (this->paths.size() == 1)
	{
		try
		{
			auto data = this->search_bytes_in_file(this->tofind, this->paths.at(0), slice_size, progress);
			std::unordered_map<Path, std::vector<PositionsInFile>> umap{ {std::move(this->paths.at(0)), this->search_bytes_in_file(this->tofind, this->paths.at(0), slice_size, progress)} };
			auto res = SearchRes{ std::move(this->tofind), std::move(umap), {} };
			this->reset();
			return res;
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what();
			unopened_files.push_back(std::move(this->paths.at(0)));
		}

		return SearchRes(std::move(this->tofind), std::move(unopened_files));
	}

	std::vector<std::future<std::pair<std::vector<std::pair<unsigned, std::vector<PositionsInFile>>>, UnopenedFiles>>> futures{};
	futures.reserve(threads_number);

	auto func = [this, slice_size, &progress](const std::vector<unsigned>& indexes) {
		std::pair<std::vector<std::pair<unsigned, std::vector<PositionsInFile>>>, UnopenedFiles> result{};
		result.first.reserve(indexes.size());
		for (auto i : indexes)
		{
			const auto& path = this->paths.at(i);
			try
			{
				result.first.emplace_back(i, this->search_bytes_in_file(this->tofind, path, slice_size, progress));
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what();
				result.second->push_back(path);
			}
		}

		return result;
	};

	for (const auto& indexes : paths_indexes)
		futures.push_back(std::move(std::async(std::launch::async, func, indexes)));
	
	for (auto future_it = futures.begin(); future_it != futures.end(); ++future_it)
	{
		auto tmp = future_it->get();
		if (tmp.second)
		{
			for (auto& unopened_file : tmp.second.value())
				unopened_files.push_back(std::move(unopened_file));
		}
		for (auto& result_for_file : tmp.first)
			data.emplace(this->paths.at(result_for_file.first), std::move(result_for_file.second));
	}

	return unopened_files.size() == paths.size() ? SearchRes{std::move(this->tofind), std::move(unopened_files)} : SearchRes{ std::move(this->tofind), std::move(data), std::move(unopened_files) };
}
std::vector<PositionsInFile> Search::search_bytes_in_file(const RawBytesSet& hexes, const Path& path, size_t slice_size, std::atomic<unsigned>& progress)
{
	if (!fs::exists(path) || !fs::is_regular_file(path)) 
		throw std::logic_error("Invalid path");
	std::vector<PositionsInFile> result(hexes.size());
	if (hexes.empty()) 
		return result;

	auto hex_max_size = std::max_element(hexes.cbegin(), hexes.cend())->get().size();
	slice_size = (hex_max_size > slice_size) ? hex_max_size : slice_size;
	IfstreamSlicer file{ path, slice_size };

	std::vector<uintmax_t> min_next_occur_pos(hexes.size(), 0);
	auto it_hex = hexes.cbegin();
	for (auto pos = file.tellg(); pos != file.size(); pos = file.tellg())
	{
		auto hex_index = std::distance(hexes.cbegin(), it_hex);
		bool found = true;
		auto buff_pos = pos;
		for (char ch : it_hex->get())
		{
			auto opt_ch = file.get();
			if (!opt_ch || ((file.size() - pos) < it_hex->size()) || (pos < min_next_occur_pos.at(hex_index)) || (ch != opt_ch.value()))
			{
				found = false;
				break;
			}
		}
		if (found)
		{
			result.at(hex_index).push_back(pos);
			min_next_occur_pos.at(hex_index) = pos + it_hex->size();
		}
		++it_hex;
		if (it_hex == hexes.cend())
		{
			it_hex = hexes.cbegin();
			++buff_pos;
		}
		file.seekg_prev(buff_pos);
		//file.seekg_prev(++pos);
	}
	++progress;

	return result;
}
void Search::sort_paths()
{
	std::sort(this->paths.begin(), this->paths.end());
	auto last = std::unique(this->paths.begin(), this->paths.end());
	this->paths.erase(last, this->paths.end());
	std::sort(this->paths.begin(), this->paths.end(), [](const Path& p1, const Path& p2) { return fs::file_size(p1) > fs::file_size(p2); }); // < or > ?
}
std::vector<std::vector<unsigned>> Search::group_paths_for_threads()
{
	sort_paths();
	this->threads_number = paths.size() >= this->threads_number ? this->threads_number : paths.size();
	std::vector<std::vector<unsigned>> groups_of_paths_indexes(this->threads_number, std::vector<unsigned>{});
	std::vector<uintmax_t> filessize_per_thread(this->threads_number, 0);
	auto get_group = [&filessize_per_thread](const Path& p) {
		auto filesize = fs::file_size(p);
		if (filessize_per_thread.empty())
		{
			*filessize_per_thread.begin() += filesize;
			return unsigned(0);
		}
		auto it = std::min_element(filessize_per_thread.begin(), filessize_per_thread.end());
		*it += filesize;
		return static_cast<unsigned>(std::distance(filessize_per_thread.begin(), it));
	};
	for (auto it = this->paths.cbegin(); it != this->paths.cend(); ++it)
		groups_of_paths_indexes.at(get_group(*it)).push_back(std::distance(this->paths.cbegin(), it));
	
	return groups_of_paths_indexes;
}

