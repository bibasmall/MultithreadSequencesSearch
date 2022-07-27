#pragma once
#include <iterator>
#include <memory>
#include <unordered_set>
#include <optional>
#include <fstream>
#include <functional>
#include <atomic>

class RawBytes;
struct RawBytesHasher;
struct RawBytesComparator;
using RawBytesIt = std::vector<char>::iterator;
using RawBytesSet = std::unordered_set<RawBytes, RawBytesHasher>;
using Path = std::wstring;
using PositionsInFile = std::vector<uintmax_t>;
using ProgressCallback = std::function<void(unsigned)>;
using UnopenedFiles = std::optional<std::vector<Path>>;


struct __declspec(dllexport)RawBytesHasher
{
	std::size_t operator()(const RawBytes&) const;
};

class __declspec(dllexport) RawBytes 
{
public:
	RawBytes() = default;
	RawBytes(const RawBytes&) = default;
	RawBytes(RawBytes&&) = default;
	~RawBytes() = default;
	RawBytes& operator=(const RawBytes&) = default;
	RawBytes& operator=(RawBytes&& other) = default;

	static std::optional<RawBytes> make_hex(std::string_view) noexcept;
	RawBytes(std::vector<char> src) : seq{ src } {	}
	template <class T>
	RawBytes(const std::basic_string<T>& str)
	{
		const char* ch = reinterpret_cast<const char*>(&str[0]);
		std::size_t size_ = str.size() * sizeof(str.front());
		std::vector<char> data(ch, ch + size_);
		this->seq = std::move(data);
	}
	bool operator==(const RawBytes&) const noexcept;
	bool operator<(const RawBytes&) const noexcept;
	inline const std::vector<char>& get() const noexcept;
	inline size_t size() const noexcept;

private:
	RawBytes(std::string_view);

	static RawBytesIt shrink_bits_base(RawBytesIt first, RawBytesIt last);
	static bool is_hex(char) noexcept;
	static char to_hex(char) noexcept;

	void shrink_and_resize();

private:
	std::vector<char> seq;
};
__declspec(dllexport) std::ostream& operator<<(std::ostream&, const RawBytes&);
__declspec(dllexport) std::wostream& operator<<(std::wostream&, const RawBytes&);


class __declspec(dllexport) IfstreamSlicer
{
public:
	IfstreamSlicer() = delete;
	IfstreamSlicer(const IfstreamSlicer&) = delete;
	IfstreamSlicer(IfstreamSlicer&& other) = default;
	~IfstreamSlicer() = default;
	IfstreamSlicer& operator=(const IfstreamSlicer&) = delete;
	IfstreamSlicer& operator=(IfstreamSlicer&&) = default;

	IfstreamSlicer(const Path&, uintmax_t);

	bool good() const noexcept;
	uintmax_t size() const;
	uintmax_t tellg() const;

	std::optional<char> get() noexcept;
	void seekg_prev(uintmax_t);

protected:
	bool end() const;

	std::vector<char> slice_head;
	std::vector<char> slice_tail;

private:
	std::ifstream file;
	uintmax_t file_size;
	unsigned slice_cursor;
	uintmax_t file_cursor;
	bool noslice;
};


class __declspec(dllexport) SearchRes
{
public:
	SearchRes() = default;
	SearchRes(const SearchRes&) = delete;
	SearchRes(SearchRes&&) = default;
	~SearchRes() = default;
	SearchRes& operator=(const SearchRes&) = delete;
	SearchRes& operator=(SearchRes&&) = default;

	SearchRes(RawBytesSet, std::unordered_map<Path, std::vector<PositionsInFile>>, UnopenedFiles);
	SearchRes(RawBytesSet, UnopenedFiles);

	RawBytesSet::const_iterator rowbytes_cbegin() const;
	RawBytesSet::const_iterator rowbytes_cend() const;

	const PositionsInFile& at(const Path&, const RawBytes&) const;
	bool contains(const Path&) const;
	std::vector<Path> collect_paths() const noexcept;
	const UnopenedFiles& unopened_files() const noexcept;
	bool empty() const noexcept;

	void reset() noexcept;


private:
	RawBytesSet tofind{};
	std::unordered_map<Path, std::vector<PositionsInFile>> data{};
	UnopenedFiles skipped;
};


class __declspec(dllexport) Search
{
	RawBytesSet tofind = {};
	std::vector<Path> paths = {};
	unsigned threads_number = std::thread::hardware_concurrency() - 1;

public:
	Search() = default;
	Search(const Search&) = delete;
	Search(Search&&) = default;
	~Search() = default;
	Search& operator=(const Search&) = delete;
	Search& operator=(Search&&) = default;

	Search(Path, RawBytesSet = {});

	size_t size() const noexcept;
	bool add_bytes(RawBytes) noexcept;
	bool add_path(Path) noexcept;
	void reset() noexcept;

	bool ready() const noexcept;

	SearchRes exec_and_reset(size_t, std::atomic<unsigned>&);

private:
	static std::vector<PositionsInFile> search_bytes_in_file(const RawBytesSet&, const Path&, size_t, std::atomic<unsigned>& progress);
	
	void sort_paths();
	std::vector<std::vector<unsigned>> group_paths_for_threads();

};