create table (
	day varchar(10) not null,
	total int ,
	success int,
	fail int,
	process_time varchar(16) not null,
	primary key (day)
)ENGINE=InnoDb DEFAULT CHARSET=latin1; 
