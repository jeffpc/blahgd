CREATE TABLE posts (
	id INTEGER PRIMARY KEY,
	time TIMESTAMP,
	title VARCHAR,
	fmt INTEGER
);

CREATE TABLE tags (
	tag VARCHAR PRIMARY KEY
);

CREATE TABLE post_tags (
	post INTEGER,
	tag VARCHAR,
	PRIMARY KEY(post, tag),
	FOREIGN KEY(post) REFERENCES posts(id),
	FOREIGN KEY(tag) REFERENCES tags(tag)
);

CREATE TABLE cats (
	cat VARCHAR PRIMARY KEY
);

CREATE TABLE post_cats (
	post INTEGER,
	cat VARCHAR,
	PRIMARY KEY(post, cat),
	FOREIGN KEY(post) REFERENCES posts(id),
	FOREIGN KEY(cat) REFERENCES cats(cat)
);

CREATE TABLE comments (
	post INTEGER,
	id INTEGER,
	author VARCHAR,
	email VARCHAR,
	time TIMESTAMP,
	remote_addr VARCHAR,
	url VARCHAR,
	moderated INTEGER,
	PRIMARY KEY(post, id),
	FOREIGN KEY(post) REFERENCES posts(id)
);
