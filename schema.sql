CREATE TABLE posts (
	id INTEGER PRIMARY KEY,
	time TIMESTAMP,
	title VARCHAR,
	fmt INTEGER
);

CREATE TABLE post_tags (
	post INTEGER,
	tag VARCHAR,
	PRIMARY KEY(post, tag),
	FOREIGN KEY(post) REFERENCES posts(id)
);

CREATE TABLE post_cats (
	post INTEGER,
	cat VARCHAR,
	PRIMARY KEY(post, cat),
	FOREIGN KEY(post) REFERENCES posts(id)
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

-- cached sidebar info
CREATE TABLE tagcloud (
	tag VARCHAR,
	cnt INTEGER,
	PRIMARY KEY(tag)
);

CREATE INDEX tagcloud_idx on tagcloud (tag COLLATE NOCASE ASC);

CREATE TRIGGER update_tagcloud_1 AFTER INSERT ON post_tags
BEGIN
	INSERT OR REPLACE INTO tagcloud (tag, cnt)
		SELECT tag, count(1) as cnt FROM post_tags WHERE tag = NEW.tag GROUP BY tag;
END;

CREATE TRIGGER update_tagcloud_2 AFTER DELETE ON post_tags
BEGIN
	INSERT OR REPLACE INTO tagcloud (tag, cnt)
		SELECT tag, count(1) as cnt FROM post_tags WHERE tag = OLD.tag GROUP BY tag;
END;

CREATE TRIGGER update_tagcloud_3 AFTER UPDATE ON post_tags
BEGIN
	INSERT OR REPLACE INTO tagcloud (tag, cnt)
		SELECT tag, count(1) as cnt FROM post_tags WHERE tag = NEW.tag GROUP BY tag;
	INSERT OR REPLACE INTO tagcloud (tag, cnt)
		SELECT tag, count(1) as cnt FROM post_tags WHERE tag = OLD.tag GROUP BY tag;
END;
