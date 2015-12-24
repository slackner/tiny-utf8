#include "tinyutf8.h"

utf8_string::utf8_string( utf8_string::size_type number , utf8_string::value_type ch ) :
	string_len( number )
	, indices_of_multibyte( ch <= 0x7F && number ? nullptr : new utf8_string::size_type[number] )
	, indices_len( ch <= 0x7F ? 0 : number )
	, misformated( false )
	, static_buffer( false )
{
	if( !number ){
		this->buffer = nullptr;
		this->buffer_len = 0;
		return;
	}
	
	unsigned char num_bytes = get_num_bytes_of_utf8_codepoint( ch );
	this->buffer_len		= ( ch ? number * num_bytes : 0 ) + 1;
	this->buffer			= new char[this->buffer_len];
	
	if( num_bytes == 1 )
		while( number-- > 0 )
			this->buffer[number] = ch;
	else
	{
		char representation[6];
		encode_utf8( ch , representation );
		
		while( number-- > 0 )
		{
			unsigned char byte = num_bytes;
			while( byte-- > 0 )
				this->buffer[ number * num_bytes + byte ] = representation[byte];
			this->indices_of_multibyte[number] = number * num_bytes;
		}
	}
	
	// Set trailling zero
	this->buffer[ this->buffer_len - 1 ] = 0;
}

utf8_string::utf8_string( const char* str , size_type len ) :
	utf8_string()
{
	if( str && str[0] )
	{
		size_type		num_multibytes = 0;
		size_type		i = 0;
		unsigned char	bytesToSkip = 0;
		
		// Count Multibytes
		for( ; str[i] && this->string_len < len ; i++ )
		{
			string_len++; // Count number of codepoints
			
			if( str[i] <= 0x7F ) // lead bit is zero, must be a single ascii
				continue;
			
			// Compute the number of bytes to skip
			bytesToSkip = get_num_bytes_of_utf8_char( str , i ) - 1;
			
			num_multibytes++; // Increase counter
			
			// Check if the string doesn't end just right inside the multibyte part!
			do{
				if( !str[i+1] || (str[i+1] & 0xC0) != 0x80 ){
					this->misformated = true;
					break;
				}
				i++;
			}while( --bytesToSkip > 0 );
		}
		
		// Initialize buffer
		this->buffer_len = i + 1;
		this->buffer = new char[this->buffer_len];
		std::memcpy( this->buffer , str , this->buffer_len );
		
		// initialize indices table
		this->indices_len = num_multibytes;
		this->indices_of_multibyte = this->indices_len ? new size_type[this->indices_len] : nullptr;
		
		size_type multibyteIndex = 0;
		
		// Fill Multibyte Table
		for( size_type index = 0 ; index < i ; index++ )
		{
			if( buffer[index] <= 0x7F ) // lead bit is zero, must be a single ascii
				continue;
			
			// Compute number of bytes to skip
			bytesToSkip = get_num_bytes_of_utf8_char( buffer , index ) - 1;
			
			this->indices_of_multibyte[multibyteIndex++] = index;
			
			if( this->misformated ){
				do{
					if( index + 1 == i || (buffer[index+1] & 0xC0) != 0x80 )
						break;
					index++;
				}while( --bytesToSkip > 0 );
			}
			else
				index += bytesToSkip;
		}
	}
}

utf8_string::utf8_string( const value_type* str ) :
	utf8_string()
{
	if( str && str[0] )
	{
		size_type	num_multibytes;
		size_type	nextIndexPosition = 0;
		size_type	string_len = 0;
		size_type	num_bytes = get_num_required_bytes( str , num_multibytes );
		
		// Initialize the internal buffer
		this->buffer_len = num_bytes + 1; // +1 for the trailling zero
		this->buffer = new char[this->buffer_len];
		this->indices_len = num_multibytes;
		this->indices_of_multibyte = this->indices_len ? new size_type[this->indices_len] : nullptr;
		
		char* curWriter = this->buffer;
		
		// Iterate through wide char literal
		for( size_type i = 0; str[i] ; i++ )
		{
			// Encode wide char to utf8
			unsigned char num_bytes = encode_utf8( str[i] , curWriter );
			
			// Push position of character to 'indices_of_multibyte'
			if( num_bytes > 1 )
				this->indices_of_multibyte[nextIndexPosition++] = curWriter - this->buffer;
			
			// Step forward with copying to internal utf8 buffer
			curWriter += num_bytes;
			
			// Increase string length by 1
			string_len++;
		}
		
		this->string_len = string_len;
		
		// Add trailling \0 char to utf8 buffer
		*curWriter = 0;
	}
}




utf8_string::utf8_string( utf8_string&& str ) :
	buffer( str.buffer )
	, buffer_len( str.buffer_len )
	, string_len( str.string_len )
	, indices_of_multibyte( str.indices_of_multibyte )
	, indices_len( str.indices_len )
	, misformated( str.misformated )
	, static_buffer( str.static_buffer )
{
	// Reset old string
	str.buffer = nullptr;
	str.indices_of_multibyte = nullptr;
	str.indices_len = 0;
	str.buffer_len = 0;
	str.string_len = 0;
}

utf8_string::utf8_string( const utf8_string& str ) :
	buffer( str.empty() ? nullptr : new char[str.buffer_len] )
	, buffer_len( str.buffer_len )
	, string_len( str.string_len )
	, indices_of_multibyte( str.indices_len ? new size_type[str.indices_len] : nullptr )
	, indices_len( str.indices_len )
	, misformated( str.misformated )
	, static_buffer( false )
{
	// Clone data
	if( !str.empty() )
		std::memcpy( this->buffer , str.buffer , str.buffer_len );
	
	// Clone indices
	if( str.indices_len )
		std::memcpy( this->indices_of_multibyte , str.indices_of_multibyte , str.indices_len * sizeof(size_type) );
}




utf8_string& utf8_string::operator=( const utf8_string& str )
{
	if( str.empty() ){
		clear();
		return *this;
	}
	
	char*		newBuffer = new char[str.buffer_len];
	size_type*	newIndices = str.indices_len ? new size_type[str.indices_len] : nullptr;
	
	// Clone data
	std::memcpy( newBuffer , str.buffer , str.buffer_len );
	
	// Clone indices
	if( str.indices_len > 0 )
		std::memcpy( newIndices , str.indices_of_multibyte , str.indices_len * sizeof(size_type) );
	
	this->misformated = str.misformated;
	this->string_len = str.string_len;
	reset_buffer( newBuffer , str.buffer_len );
	reset_indices( newIndices , str.indices_len );
	return *this;
}

utf8_string& utf8_string::operator=( utf8_string&& str )
{
	// Copy data
	reset_buffer( str.buffer , str.buffer_len );
	reset_indices( str.indices_of_multibyte , str.indices_len );
	this->misformated = str.misformated;
	this->string_len = str.string_len;
	this->static_buffer = str.static_buffer;
	
	// Reset old string
	str.indices_of_multibyte = nullptr;
	str.indices_len = 0;
	str.buffer = nullptr;
	str.buffer_len = 0;
	str.string_len = 0;
	return *this;
}


unsigned char utf8_string::get_num_bytes_of_utf8_char_before( const char* data , size_type current_byte_index ) const
{
	if( current_byte_index >= size() || !this->requires_unicode() )
		return 1;
	
	data += current_byte_index;
	
	// If we know the utf8 string is misformated, we have to check, how many 
	if( this->misformated )
	{
		// Check if byte is data-blob
		if( (data[-1] & 0xC0) != 0x80 )	return 1;
		// 110XXXXX - two bytes?
		if( (data[-2] & 0xE0) == 0xC0 )	return 2;
		
		// Check if byte is data-blob
		if( (data[-2] & 0xC0) != 0x80 )	return 2;
		// 1110XXXX - three bytes?
		if( (data[-3] & 0xF0) == 0xE0 )	return 3;
		
		// Check if byte is data-blob
		if( (data[-3] & 0xC0) != 0x80 )	return 3;
		// 11110XXX - four bytes?
		if( (data[-4] & 0xF8) == 0xF0 )	return 4;
		
		// Check if byte is data-blob
		if( (data[-4] & 0xC0) != 0x80 )	return 4;
		// 111110XX - five bytes?
		if( (data[-5] & 0xFC) == 0xF8 )	return 5;
		
		// Check if byte is data-blob
		if( (data[-5] & 0xC0) != 0x80 )	return 5;
		// 1111110X - six bytes?
		if( (data[-6] & 0xFE) == 0xFC )	return 6;
	}
	else
	{
		// Check if byte is no data-blob
		if( (data[-1] & 0xC0) != 0x80 )
			return 1;
		else if( (data[-2] & 0xE0) == 0xC0 )  // 110XXXXX  two bytes
			return 2;
		else if( (data[-3] & 0xF0) == 0xE0 )  // 1110XXXX  three bytes
			return 3;
		else if( (data[-4] & 0xF8) == 0xF0 )  // 11110XXX  four bytes
			return 4;
		else if( (data[-5] & 0xFC) == 0xF8 )  // 111110XX  five bytes
			return 5;
		else if( (data[-6] & 0xFE) == 0xFC )  // 1111110X  six bytes
			return 6;
	}
	return 1;
}



unsigned char utf8_string::get_num_bytes_of_utf8_char( const char* data , size_type first_byte_index ) const
{
	if( this->buffer_len <= first_byte_index )
		return 1;
	
	data += first_byte_index;
	unsigned char first_byte = data[0];
	
	// If we know the utf8 string is misformated, we have to check, how many 
	if( this->misformated )
	{
		if( first_byte <= 0x7F )
			return 1;
		
		// Check if byte is data-blob
		if( (data[1] & 0xC0) != 0x80 )		return 1;
		// 110XXXXX - two bytes?
		if( (first_byte & 0xE0) == 0xC0 )	return 2;
		
		// Check if byte is data-blob
		if( (data[2] & 0xC0) != 0x80 )		return 2;
		// 1110XXXX - three bytes?
		if( (first_byte & 0xF0) == 0xE0 )	return 3;
		
		// Check if byte is data-blob
		if( (data[3] & 0xC0) != 0x80 )		return 3;
		// 11110XXX - four bytes?
		if( (first_byte & 0xF8) == 0xF0 )	return 4;
		
		// Check if byte is data-blob
		if( (data[4] & 0xC0) != 0x80 )		return 4;
		// 111110XX - five bytes?
		if( (first_byte & 0xFC) == 0xF8 )	return 5;
		
		// Check if byte is data-blob
		if( (data[5] & 0xC0) != 0x80 )		return 5;
		// 1111110X - six bytes?
		if( (first_byte & 0xFE) == 0xFC )	return 6;
	}
	else
	{
		if( first_byte <= 0x7F )
			return 1;
		else if( (first_byte & 0xE0) == 0xC0 )  // 110XXXXX  two bytes
			return 2;
		else if( (first_byte & 0xF0) == 0xE0 )  // 1110XXXX  three bytes
			return 3;
		else if( (first_byte & 0xF8) == 0xF0 )  // 11110XXX  four bytes
			return 4;
		else if( (first_byte & 0xFC) == 0xF8 )  // 111110XX  five bytes
			return 5;
		else if( (first_byte & 0xFE) == 0xFC )  // 1111110X  six bytes
			return 6;
	}
	return 1; // one byte
}



unsigned char utf8_string::decode_utf8( const char* data , value_type& dest ) const
{
	unsigned char first_char = *data;
	
	if( !first_char ){
		dest = 0;
		return 1;
	}
	
	value_type		codepoint;
	unsigned char	num_bytes;
	
	if( first_char <= 0x7F ){ // 0XXX XXXX one byte
		dest = first_char;
		return 1;
	}
	else if( (first_char & 0xE0) == 0xC0 ){  // 110X XXXX  two bytes
		codepoint = first_char & 31;
		num_bytes = 2;
	}
	else if( (first_char & 0xF0) == 0xE0 ){  // 1110 XXXX  three bytes
		codepoint = first_char & 15;
		num_bytes = 3;
	}
	else if( (first_char & 0xF8) == 0xF0 ){  // 1111 0XXX  four bytes
		codepoint = first_char & 7;
		num_bytes = 4;
	}
	else if( (first_char & 0xFC) == 0xF8 ){  // 1111 10XX  five bytes
		codepoint = first_char & 3;
		num_bytes = 5;
		misformated = true;
	}
	else if( (first_char & 0xFE) == 0xFC ){  // 1111 110X  six bytes
		codepoint = first_char & 1;
		num_bytes = 6;
		misformated = true;
	}
	else{ // not a valid first byte of a UTF-8 sequence
		codepoint = first_char;
		num_bytes = 1;
		misformated = true;
	}
	
	if( !misformated )
		for( int i = 1 ; i < num_bytes ; i++ )
			codepoint = (codepoint << 6) | (data[i] & 0x3F);
	else
		for( int i = 1 ; i < num_bytes ; i++ ){
			if( !data[i] || (data[i] & 0xC0) != 0x80 ){
				misformated = true;
				num_bytes = i;
				if( i == 1 )
					codepoint = first_char;
				else
					codepoint = fallback_codepoint;
				break;
			}
			codepoint = (codepoint << 6) | (data[i] & 0x3F);
		}
	
	dest = codepoint;
	
	return num_bytes;
}




utf8_string::size_type utf8_string::get_num_required_bytes( const value_type* lit , size_type& num_multibytes )
{
	size_type num_bytes = 0;
	num_multibytes = 0;
	for( size_type i = 0 ; lit[i] ; i++ ){
		unsigned char cur_bytes = get_num_bytes_of_utf8_codepoint( lit[i] );
		if( cur_bytes > 1 )
			num_multibytes++;
		num_bytes += cur_bytes;
	}
	return num_bytes;
}

utf8_string::size_type utf8_string::get_num_resulting_codepoints( size_type byte_start , size_type byte_count ) const 
{
	if( empty() )
		return 0;
	
	size_type	curIndex = byte_start;
	size_type	codepoint_count = 0;
	
	byte_count = std::min<difference_type>( byte_count , size() - byte_start );
	
	while( byte_count > 0 ){
		unsigned char num_bytes = get_num_bytes_of_utf8_char( this->buffer , curIndex );
		curIndex += num_bytes;
		byte_count -= num_bytes;
		codepoint_count++;
	}
	return codepoint_count;
}

utf8_string::size_type utf8_string::get_num_resulting_bytes( size_type byte_start , size_type codepoint_count ) const 
{
	if( empty() )
		return 0;
	
	if( !requires_unicode() )
		return std::min<size_type>( byte_start + codepoint_count , size() ) - byte_start;
	
	size_type cur_byte = byte_start;
	
	while( cur_byte < size() && codepoint_count-- > 0 )
		cur_byte += get_index_bytes( cur_byte );
	
	return cur_byte - byte_start;
}




unsigned char utf8_string::encode_utf8( value_type codepoint , char* dest )
{
	if( codepoint <= 0x7F ){ // 0XXXXXXX one byte
		dest[0] = char(codepoint);
		return 1;
	}
	if( codepoint <= 0x7FF ){ // 110XXXXX  two bytes
		dest[0] = char( 0xC0 | (codepoint >> 6) );
		dest[1] = char( 0x80 | (codepoint & 0x3F) );
		return 2;
	}
	if( codepoint <= 0xFFFF ){ // 1110XXXX  three bytes
		dest[0] = char( 0xE0 | (codepoint >> 12) );
		dest[1] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[2] = char( 0x80 | (codepoint & 0x3F) );
		return 3;
	}
	if( codepoint <= 0x1FFFFF ){ // 11110XXX  four bytes
		dest[0] = char( 0xF0 | (codepoint >> 18) );
		dest[1] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		dest[2] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[3] = char( 0x80 | (codepoint & 0x3F) );
		return 4;
	}
	if( codepoint <= 0x3FFFFFF ){ // 111110XX  five bytes
		dest[0] = char( 0xF8 | (codepoint >> 24) );
		dest[1] = char( 0x80 | (codepoint >> 18) );
		dest[2] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		dest[3] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[4] = char( 0x80 | (codepoint & 0x3F) );
		return 5;
	}
	if( codepoint <= 0x7FFFFFFF ){ // 1111110X  six bytes
		dest[0] = char( 0xFC | (codepoint >> 30) );
		dest[1] = char( 0x80 | ((codepoint >> 24) & 0x3F) );
		dest[2] = char( 0x80 | ((codepoint >> 18) & 0x3F) );
		dest[3] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		dest[4] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[5] = char( 0x80 | (codepoint & 0x3F) );
		return 6;
	}
	
	return 1;
}




utf8_string::size_type utf8_string::get_actual_index( size_type requested_index ) const
{
	if( requested_index >= this->string_len )
		return this->buffer_len - 1;
	
	size_type index_multibyte_table = 0;
	size_type currentOverhead = 0;
	
	while( index_multibyte_table < this->indices_len )
	{
		size_type multibyte_pos = this->indices_of_multibyte[index_multibyte_table];
		
		if( multibyte_pos >= requested_index + currentOverhead )
			break;
		
		index_multibyte_table++;
		currentOverhead += get_num_bytes_of_utf8_char( this->buffer , multibyte_pos ) - 1; // Ad bytes of multibyte to overhead
	}
	
	return requested_index + currentOverhead;
}

utf8_string utf8_string::raw_substr( size_type byte_index , size_type tmpByteCount , size_type numCodepoints ) const
{
	if( byte_index > size() )
		return utf8_string();
	
	difference_type byte_count;
	if( tmpByteCount < size() - byte_index )
		byte_count = tmpByteCount;
	else
		byte_count = size() - byte_index;
	
	size_type	actualStartIndex = byte_index;
	size_type	actualEndIndex = byte_index + byte_count;
	
	//
	// Manage indices
	//
	size_type	startOfMultibytesInRange;
	
	// Look for start of indices
	size_type tmp = 0;
	while( tmp < this->indices_len && this->indices_of_multibyte[tmp] < actualStartIndex )
		tmp++;
	startOfMultibytesInRange = tmp;
	
	// Look for the end
	while( tmp < this->indices_len && this->indices_of_multibyte[tmp] < actualEndIndex )
		tmp++;
	
	// Compute number of indices 
	size_type	newIndicesLen = tmp - startOfMultibytesInRange;
	size_type*	newIndices = newIndicesLen ? new size_type[newIndicesLen] : nullptr;
	
	// Copy indices
	for( size_type i = 0 ; i < newIndicesLen ; i++ )
		newIndices[i] = this->indices_of_multibyte[startOfMultibytesInRange + i] - actualStartIndex;
	//
	// Manage utf8 string
	//
	
	// Allocate new buffer
	char*	newBuffer = new char[byte_count+1];
	
	// Partly copy
	std::memcpy( newBuffer , this->buffer + actualStartIndex , byte_count );
	
	newBuffer[byte_count] = 0;
	byte_count++;
	
	return utf8_string( newBuffer , byte_count , numCodepoints , newIndices , newIndicesLen );
}

#include <stdio.h>

void utf8_string::raw_replace( size_type actualStartIndex , size_type replacedBytes , const utf8_string& replacement )
{
	if( actualStartIndex > size() )
		actualStartIndex = size();
	
	size_type		replacementBytes = replacement.size();
	size_type		replacementIndices = replacement.indices_len;
	difference_type	byteDifference = replacedBytes - replacementBytes;
	size_type		actualEndIndex;
	
	if( replacedBytes < size() - actualStartIndex )
		actualEndIndex = actualStartIndex + replacedBytes;
	else{
		actualEndIndex = size();
		replacedBytes = actualEndIndex - actualStartIndex;
	}
	
	//
	// Manage indices
	//
	
	size_type	startOfMultibytesInRange;
	size_type	numberOfMultibytesInRange;
	
	// Look for start of indices
	size_type tmp = 0;
	while( tmp < this->indices_len && this->indices_of_multibyte[tmp] < actualStartIndex )
		tmp++;
	startOfMultibytesInRange = tmp;
	
	// Look for the end
	while( tmp < this->indices_len && this->indices_of_multibyte[tmp] < actualEndIndex )
		tmp++;
	
	// Compute number of indices 
	numberOfMultibytesInRange = tmp - startOfMultibytesInRange;
	
	// Compute difference in number of indices
	difference_type indicesDifference = numberOfMultibytesInRange - replacementIndices;
	
	// Create new buffer
	size_type newIndicesLen = this->indices_len - indicesDifference;
	
	if( newIndicesLen )
	{
		size_type*	newIndices = newIndicesLen ? new size_type[ newIndicesLen ] : nullptr;
		
		// Copy values before replacement start
		for( size_type i = 0 ; i < this->indices_len && this->indices_of_multibyte[i] < actualStartIndex ; i++ )
			newIndices[i] = this->indices_of_multibyte[i];
		
		// Copy the values after the replacement
		if( indicesDifference != 0 ){
			for(
				size_type i = startOfMultibytesInRange
				; i + numberOfMultibytesInRange < this->indices_len
				; i++
			)
				newIndices[ i + replacementIndices ] = this->indices_of_multibyte[ i + numberOfMultibytesInRange ] - byteDifference;
		}
		else{
			size_type i = startOfMultibytesInRange + numberOfMultibytesInRange;
			while( i < this->indices_len ){
				newIndices[i] = this->indices_of_multibyte[i] - byteDifference;
				i++;
			}
		}
		
		// Insert indices from replacement
		for( size_type i = 0 ; i < replacementIndices ; i++ )
			newIndices[ startOfMultibytesInRange + i ] = replacement.indices_of_multibyte[i] + actualStartIndex;
		
		// Move and reset
		reset_indices( newIndices , newIndicesLen );
	}
	else
		// Reset because empty
		reset_indices();
	
	//
	// Manage utf8 data
	//
	
	// Allocate new buffer
	size_type normalized_buffer_len = std::max<size_type>( this->buffer_len , 1 );
	size_type newBufferLen = normalized_buffer_len + ( replacementBytes - replacedBytes );
	
	if( newBufferLen > 1 )
	{
		char* newBuffer = new char[newBufferLen];
		
		// Partly copy
		// Copy string until replacement index
		std::memcpy( newBuffer , this->buffer , actualStartIndex );
		
		// Copy rest
		std::memcpy(
			newBuffer + actualStartIndex + replacementBytes
			, this->buffer + actualEndIndex
			, normalized_buffer_len - actualEndIndex - 1 // ('-1' for skipping the trailling zero which is not present in case the buffer_len was 0)
		);
		
		// Set trailling zero
		newBuffer[ newBufferLen - 1 ] = '\0';
		
		// Write bytes
		std::memcpy( newBuffer + actualStartIndex , replacement.buffer , replacementBytes );
		
		// Adjust string length
		this->string_len -= get_num_resulting_codepoints( actualStartIndex , replacedBytes );
		this->string_len += replacement.length();
		
		// Rewrite buffer
		reset_buffer( newBuffer , newBufferLen );
		
		this->misformated = this->misformated || replacement.misformated;
	}
	else
		// Reset because empty
		clear();
}




utf8_string::value_type utf8_string::at( size_type requested_index ) const
{
	if( requested_index >= size() )
		return 0;
	
	if( !requires_unicode() )
		return (value_type) this->buffer[requested_index];
	
	// Decode internal buffer at position n
	value_type codepoint = 0;
	decode_utf8( this->buffer + get_actual_index( requested_index ) , codepoint );
	
	return codepoint;
}




std::unique_ptr<utf8_string::value_type[]> utf8_string::toWideLiteral() const
{
	if( empty() )
		return std::unique_ptr<value_type[]>( new value_type[1]{0} );
	
	std::unique_ptr<value_type[]>	dest = std::unique_ptr<value_type[]>( new value_type[length()] );
	value_type*						tempDest = dest.get();
	char*							source = this->buffer;
	
	while( *source )
		source += decode_utf8( source , *tempDest++ );
	
	return std::move(dest);
}




utf8_string::difference_type utf8_string::compare( const utf8_string& str ) const
{
	const_iterator	it1 = cbegin();
	const_iterator	it2 = str.cbegin();
	const_iterator	end1 = cend();
	const_iterator	end2 = str.cend();
	
	while( it1 != end1 && it2 != end2 ){
		difference_type diff = it2.get() - it1.get();
		if( diff )
			return diff;
		it1++;
		it2++;
	}
	return ( it1 == end1 ? 1 : 0 ) - ( it2 == end2 ? 1 : 0 );
}

bool utf8_string::equals( const char* str ) const
{
	const char* it1 = this->buffer;
	const char* it2 = str;
	
	if( !it1 || !it2 )
		return it1 == it2;
	
	while( *it1 && *it2 ){
		if( *it1 != *it2 )
			return false;
		it1++;
		it2++;
	}
	return *it1 == *it2;
}

bool utf8_string::equals( const value_type* str ) const
{
	const_iterator	it = cbegin();
	const_iterator	end = cend();
	
	while( it != end && *str ){
		if( *str != it.get() )
			return false;
		it++;
		str++;
	}
	return *it == *str;
}




utf8_string::size_type utf8_string::find( value_type ch , size_type start_pos ) const
{
	for( iterator it = get(start_pos) ; it < end() ; it++ )
		if( *it == ch )
			return it - begin();
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find( value_type ch , size_type byte_start ) const
{
	for( size_type it = byte_start ; it < size() ; it += get_index_bytes( it ) )
		if( raw_at(it) == ch )
			return it;
	return utf8_string::npos;
}




utf8_string::size_type utf8_string::rfind( value_type ch , size_type start_pos ) const
{
	reverse_iterator it = ( start_pos >= length() ) ? rbegin() : rget( start_pos );
	while( it < rend() ){
		if( *it == ch )
			return it - rbegin();
		it++;
	}
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_rfind( value_type ch , size_type byte_start ) const
{
	if( byte_start >= size() )
		byte_start = back_index();
	
	for( difference_type it = byte_start ; it >= 0 ; it -= get_index_pre_bytes( it ) )
		if( raw_at(it) == ch )
			return it;
	
	return utf8_string::npos;
}




utf8_string::size_type utf8_string::find_first_of( const value_type* str , size_type start_pos ) const
{
	if( start_pos >= length() )
		return utf8_string::npos;
	
	for( iterator it = get( start_pos ) ; it < end() ; it++ )
	{
		const value_type*	tmp = str;
		value_type		cur = *it;
		do{
			if( cur == *tmp )
				return it - begin();
		}while( *++tmp );
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_first_of( const value_type* str , size_type byte_start ) const
{
	if( byte_start >= size() )
		return utf8_string::npos;
	
	for( size_type it = byte_start ; it < size() ; it += get_index_bytes( it ) )
	{
		const value_type*	tmp = str;
		do{
			if( raw_at(it) == *tmp )
				return it;
		}while( *++tmp );
	}
	
	return utf8_string::npos;
}




utf8_string::size_type utf8_string::find_last_of( const value_type* str , size_type start_pos ) const
{
	reverse_iterator it = ( start_pos >= length() ) ? rbegin() : rget( start_pos );
	
	while( it < rend() )
	{
		const value_type*	tmp = str;
		value_type		cur = *it;
		do{
			if( cur == *tmp )
				return it - rbegin();
		}while( *++tmp );
		it++;
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_last_of( const value_type* str , size_type byte_start ) const
{
	if( empty() )
		return utf8_string::npos;
	
	if( byte_start >= size() )
		byte_start = back_index();
	
	for( difference_type it = byte_start ; it >= 0 ; it -= get_index_pre_bytes( it ) )
	{
		const value_type*	tmp = str;
		value_type		cur = raw_at(it);
		do{
			if( cur == *tmp )
				return it;
		}while( *++tmp );
	}
	
	return utf8_string::npos;
}




utf8_string::size_type utf8_string::find_first_not_of( const value_type* str , size_type start_pos ) const
{
	if( start_pos >= length() )
		return utf8_string::npos;
	
	for( iterator it = get(start_pos) ; it < end() ; it++ )
	{
		const value_type*	tmp = str;
		value_type		cur = *it;
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it - begin();
		
		continue2:;
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_first_not_of( const value_type* str , size_type byte_start ) const
{
	if( byte_start >= size() )
		return utf8_string::npos;
	
	for( size_type it = byte_start ; it < size() ; it += get_index_bytes( it ) )
	{
		const value_type*	tmp = str;
		value_type		cur = raw_at(it);
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it;
		
		continue2:;
	}
	
	return utf8_string::npos;
}




utf8_string::size_type utf8_string::find_last_not_of( const value_type* str , size_type start_pos ) const
{
	if( empty() )
		return utf8_string::npos;
	
	reverse_iterator it = ( start_pos >= length() ) ? rbegin() : rget( start_pos );
	
	while( it < rend() )
	{
		const value_type*	tmp = str;
		value_type		cur = *it;
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it - rbegin();
		
		continue2:
		it++;
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_last_not_of( const value_type* str , size_type byte_start ) const
{
	if( empty() )
		return utf8_string::npos;
	
	if( byte_start >= size() )
		byte_start = back_index();
	
	for( difference_type it = byte_start ; it >= 0 ; it -= get_index_pre_bytes( it ) )
	{
		const value_type*	tmp = str;
		value_type		cur = raw_at(it);
		
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it;
		
		continue2:;
	}
	
	return utf8_string::npos;
}




int operator-( const utf8_string::iterator& left , const utf8_string::iterator& right )
{
	utf8_string::size_type minIndex = std::min( left.raw_index , right.raw_index );
	utf8_string::size_type maxIndex = std::max( left.raw_index , right.raw_index );
	utf8_string::size_type numCodepoints = left.instance->get_num_resulting_codepoints( minIndex , maxIndex - minIndex );
	return maxIndex == left.raw_index ? numCodepoints : -numCodepoints;
}

utf8_string::iterator operator+( const utf8_string::iterator& it , utf8_string::size_type nth ){
	return utf8_string::iterator( it.raw_index + it.instance->get_num_resulting_bytes( it.raw_index , nth ) , it.instance );
}

int operator-( const utf8_string::reverse_iterator& left , const utf8_string::reverse_iterator& right )
{
	utf8_string::difference_type	minIndex = std::min( left.raw_index , right.raw_index );
	utf8_string::difference_type	maxIndex = std::max( left.raw_index , right.raw_index );
	utf8_string::size_type			numCodepoints = left.instance->get_num_resulting_codepoints( minIndex , maxIndex - minIndex );
	return maxIndex == right.raw_index ? numCodepoints : -numCodepoints;
}

utf8_string::reverse_iterator operator+( const utf8_string::reverse_iterator& it , utf8_string::size_type nth )
{
	utf8_string::difference_type newIndex = it.raw_index;
	while( nth-- > 0 && newIndex > 0 )
		newIndex -= it.instance->get_index_pre_bytes( newIndex );
	return utf8_string::reverse_iterator( newIndex , it.instance );
}